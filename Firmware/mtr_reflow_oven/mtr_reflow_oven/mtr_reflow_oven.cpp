#include <stdio.h>
#include <cstdlib>
#include <cmath>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"

// FreeRTOS Headers
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

// Project Headers
#include "board_config.h"
#include "project_defs.h"

// Library Headers
// #include "hagl_hal.h"

#include "lvgl.h"
#include <cwchar>
#include <cstring>
#include "ui/ui_manager.h"

// LVGL Tick Interface
extern "C" uint32_t my_tick_get(void) {
    return to_ms_since_boot(get_absolute_time());
}

#include "ff.h"
#include "diskio.h"
#include "sd_card.h"
#include "cJSON.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"

// --- Project Definitions ---
// --- Project Definitions ---
// MAX_PROFILE_SEGMENTS moved to project_defs.h

// --- Helper Functions ---
uint32_t millis() {
    return to_ms_since_boot(get_absolute_time());
}

// Helper: Play tone for passive buzzer (blocking)
void play_tone(uint32_t freq_hz, uint32_t duration_ms) {
    if (freq_hz == 0) {
        sleep_ms(duration_ms);
        return;
    }
    
    uint32_t period_us = 1000000 / freq_hz;
    uint32_t half_period = period_us / 2;
    
    uint32_t end = millis() + duration_ms;
    while (millis() < end) {
        gpio_put(GPIO_BUZZER, 1);
        sleep_us(half_period);
        gpio_put(GPIO_BUZZER, 0);
        sleep_us(half_period);
    }
}

// --- Colors ---
#define TFT_BLACK   0x0000
#define TFT_WHITE   0xFFFF
#define TFT_RED     0xF800
#define TFT_GREEN   0x07E0
#define TFT_BLUE    0x001F
#define TFT_CYAN    0x07FF
#define TFT_MAGENTA 0xF81F
#define TFT_YELLOW  0xFFE0
#define TFT_ORANGE  0xFD20
#define TFT_GREY    0x8410



// --- UI Context ---
UIContext uiCtx;

// --- WS2812B PIO Program (Pre-compiled) ---
static const uint16_t ws2812_program_instructions[] = {
    0x6221, //  0: out    x, 1            side 0 [2] 
    0x1123, //  1: jmp    !x, 3           side 1 [1] 
    0x1400, //  2: jmp    0               side 1 [4] 
    0xa442, //  3: nop                    side 0 [4] 
};

static const struct pio_program ws2812_program = {
    .instructions = ws2812_program_instructions,
    .length = 4,
    .origin = -1,
};

void ws2812_program_init(PIO pio, uint sm, uint offset, uint pin, float freq, bool rgbw) {
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_sideset_pins(&c, pin);
    sm_config_set_sideset(&c, 1, false, false); // 1 sideset bit, not optional, not pindirs
    sm_config_set_out_shift(&c, false, true, rgbw ? 32 : 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    sm_config_set_wrap(&c, offset, offset + 3); // wrap around all 4 instructions

    float div = clock_get_hz(clk_sys) / (freq * 10);
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

void put_pixel(uint32_t pixel_grb) {
    // Send to both LEDs (2 LEDs chained)
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
    sleep_us(10); // Small delay between pixels
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
    sleep_us(100); // Reset delay after last pixel
}

// --- Global Objects ---
OvenState ovenState; // Protected by mtx_OvenState
ReflowProfile currentProfile; // Global Profile Object

SystemConfig sysConfig;



// --- Mutexes & Queues ---
SemaphoreHandle_t mtx_SPI0 = NULL;
SemaphoreHandle_t mtx_I2C = NULL;
SemaphoreHandle_t mtx_OvenState = NULL;
SemaphoreHandle_t mtx_LVGL = NULL;

QueueHandle_t q_SensorData = NULL;
QueueHandle_t q_InputEvents = NULL;

// --- Interrupt Handling ---
volatile uint32_t last_irq_time = 0;

void gpio_callback(uint gpio, uint32_t events) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - last_irq_time < 50) return; // Simple debounce 50ms
    last_irq_time = now;
    
    InputEvent evt;
    evt.timestamp = now;
    evt.type = EVT_NONE;

    if (gpio == GPIO_BTN1) evt.type = EVT_BTN1_PRESS;
    else if (gpio == GPIO_BTN2) evt.type = EVT_BTN2_PRESS;

    else if (gpio == GPIO_ROT_BTN) evt.type = EVT_ENC_BTN_PRESS;
    else if (gpio == GPIO_ROT_DT) {
        // Encoder Logic (Simplified)
        if (gpio_get(GPIO_ROT_CLK)) evt.type = EVT_ENC_CCW;
        else evt.type = EVT_ENC_CW;
    } else if (gpio == GPIO_T1_ALT1) {
        // Safety Cutoff immediately (bypass queue for speed?)
        // Or just set state directly if mutex available.
        // Queue is safer for context.
        // Let's rely on vAlertHandlingTask polling software limit too.
        // But for hard interrupt:
        // Force state fault?
        // We can't take Mutex from ISR easily (xSemaphoreTakeFromISR).
        // Let's just log or set a volatile flag.
        // Ideally, we DISABLE HEATERS immediately here.
        gpio_put(GPIO_HEAT1, 0);
        gpio_put(GPIO_HEAT2, 0);
        // And send event? No input event for fault?
        // Let's make vAlertHandlingTask check the pin state too.
    }
    
    if (evt.type != EVT_NONE) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(q_InputEvents, &evt, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// --- Task Handles ---
TaskHandle_t hAlertTask = NULL;
TaskHandle_t hSensorTask = NULL;
TaskHandle_t hPIDTask = NULL;
TaskHandle_t hAppLogicTask = NULL;
TaskHandle_t hTFTDebugTask = NULL;

// --- Task Definitions ---

void vApplicationPassiveIdleHook(void) {
    // Required for SMP
}

// --- Task Functions (Core 0) ---

// --- SSR Control Logic ---
void vSSRControlTask(void *pvParameters) {
    (void)pvParameters;
    
    // Low Frequency PWM (e.g. 5Hz -> 200ms period)
    const int period_ticks = 10; // 10 * 20ms = 200ms window
    int tick_counter = 0;
    
    gpio_init(GPIO_HEAT1); gpio_set_dir(GPIO_HEAT1, GPIO_OUT);
    gpio_init(GPIO_HEAT2); gpio_set_dir(GPIO_HEAT2, GPIO_OUT);
    
    for (;;) {
        float p1 = 0, p2 = 0;
        if (xSemaphoreTake(mtx_OvenState, pdMS_TO_TICKS(10)) == pdTRUE) {
            p1 = ovenState.power_output_1; // 0-100
            p2 = ovenState.power_output_2;
            xSemaphoreGive(mtx_OvenState);
        }
        
        int threshold1 = (int)(p1 / 10.0f); // Map 0-100 to 0-10
        int threshold2 = (int)(p2 / 10.0f);
        
        gpio_put(GPIO_HEAT1, tick_counter < threshold1);
        gpio_put(GPIO_HEAT2, tick_counter < threshold2);
        
        tick_counter++;
        if (tick_counter >= period_ticks) tick_counter = 0;
        
        vTaskDelay(pdMS_TO_TICKS(20)); // 50Hz Loop
    }
}

// --- Helper to update Feedback ---
void update_feedback(OvenStateEnum state) {
    // LED Colors (GRB format) - BRIGHTER values
    uint32_t color = 0;
    switch (state) {
        case STATE_IDLE:     color = 0x00FF00; break; // Red (GRB: 0, 255, 0)
        case STATE_RUNNING:  color = 0x80FF00; break; // Orange (GRB: 128, 255, 0)
        case STATE_COOLDOWN: color = 0x0000FF; break; // Blue (GRB: 0, 0, 255)
        case STATE_FAULT:    color = 0x00FF00; break; // Red (fault = red)
        case STATE_INIT:     color = 0xFFFFFF; break; // White
        default:             color = 0; break;
    }
    put_pixel(color);
}

// Helper: Read MCP9600 temperature via raw I2C (register 0x00 = hot junction)
float read_mcp9600_temp(uint8_t addr) {
    uint8_t reg = 0x00; // Hot junction register
    uint8_t buf[2];
    
    int ret = i2c_write_blocking(I2C_PORT, addr, &reg, 1, true);
    if (ret < 0) return -999.0f;
    
    ret = i2c_read_blocking(I2C_PORT, addr, buf, 2, false);
    if (ret < 0) return -999.0f;
    
    // MCP9600: Upper byte is integer part, lower is fractional (0.0625 per bit)
    int16_t raw = (buf[0] << 8) | buf[1];
    return raw * 0.0625f;
}

// ============================================================
// === INPUT TASK (Polling & Events) ===
// ============================================================
void vInputTask(void *pvParameters) {
    (void)pvParameters;
    printf("[Input] Task started.\n");
    
    // Encoder State
    int last_clk = gpio_get(GPIO_ROT_CLK);
    static uint32_t last_enc_time = 0;
    
    // Button States (Active Low)
    int last_btn1 = 1;
    int last_btn2 = 1;

    int last_rot_btn = 1;
    
    for (;;) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        InputEvent evt;
        evt.timestamp = now;
        evt.type = EVT_NONE;
        
        // --- Button 1 ---
        int btn1 = gpio_get(GPIO_BTN1);
        if (btn1 == 0 && last_btn1 == 1) { // Pull-down Press? Or Pull-up? 
                                           // Board defined Input. Usually Pull-up -> 0 is press.
                                           // HW test showed 0=Pressed
             evt.type = EVT_BTN1_PRESS;
             play_tone(500, 50); // Audio Feedback
             xQueueSend(q_InputEvents, &evt, 0);
        }
        last_btn1 = btn1;
        
        // --- Button 2 ---
        int btn2 = gpio_get(GPIO_BTN2);
        if (btn2 == 0 && last_btn2 == 1) {
             evt.type = EVT_BTN2_PRESS;
             play_tone(500, 50);
             xQueueSend(q_InputEvents, &evt, 0);
        }
        last_btn2 = btn2;


        
        // --- Encoder Button ---
        int rot_btn = gpio_get(GPIO_ROT_BTN);
        if (rot_btn == 0 && last_rot_btn == 1) {
             evt.type = EVT_ENC_BTN_PRESS;
             play_tone(500, 50);
             xQueueSend(q_InputEvents, &evt, 0);
        }
        last_rot_btn = rot_btn;
        
        // --- Encoder Rotation ---
        int clk = gpio_get(GPIO_ROT_CLK);
        int dt = gpio_get(GPIO_ROT_DT);
        
        // Debounce 5ms
        if (clk != last_clk && (now - last_enc_time) > 5) {
            last_enc_time = now;
            // XOR logic: CLK == DT -> CCW, else CW
            if (clk == dt) {
                evt.type = EVT_ENC_CCW;
            } else {
                evt.type = EVT_ENC_CW;
            }
            // Send Encoder Event
            xQueueSend(q_InputEvents, &evt, 0);
            
            last_clk = clk;
        }
        
        // Poll Rate: 20Hz (50ms) is too slow for encoder? 
        // Encoder needs faster polling. 
        // 10ms (100Hz) might be okay for standard rotation, but 1-2ms is better.
        // HW test loop was 10Hz (100ms delay) + logic? No, check loop delay.
        // HW test loop had vTaskDelay(pdMS_TO_TICKS(100)) at end?
        // Wait, if HW test was 10Hz, encoder would be skipping like crazy!
        // Ah, vHardwareTestTask had `loop_count` check for temp, but `vTaskDelay` was 100ms? 
        // If so, the user rotated VERY slow.
        // I should increase poll rate for encoder reliability.
        vTaskDelay(pdMS_TO_TICKS(2)); // ~500Hz Polling
    }
}

void vFeedbackTask(void *pvParameters) {
    (void)pvParameters;
    
    printf("[Feedback] Task started. PIO already initialized in main().\\n");
    
    // PIO already initialized in main() - don't reinitialize!
    
    gpio_init(GPIO_BUZZER); gpio_set_dir(GPIO_BUZZER, GPIO_OUT);
    
    for (;;) {
        OvenStateEnum s = STATE_INIT;
        if (xSemaphoreTake(mtx_OvenState, pdMS_TO_TICKS(50)) == pdTRUE) {
            s = ovenState.state;
            if (s == STATE_FAULT) {
                 // Pulse 500Hz for 100ms
                 play_tone(100, 100); 
            } else {
                 gpio_put(GPIO_BUZZER, 0);
            }
            xSemaphoreGive(mtx_OvenState);
        }
        
        update_feedback(s);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void vAlertHandlingTask(void *pvParameters) {
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    for (;;) {
        // High priority: check alerts, watchdog
        // In real hardware, we would check GPIO_T1_ALT1, etc.
        
        float current_t1 = 0;
        if (xSemaphoreTake(mtx_OvenState, pdMS_TO_TICKS(10)) == pdTRUE) {
            current_t1 = ovenState.current_temp_t1;
            
            // Software Limit Check
            if (current_t1 > 260.0f) {
                ovenState.state = STATE_FAULT;
                ovenState.fault_active = true;
                ovenState.power_output_1 = 0;
                printf("!!! OVERTEMP FAULT !!!\n");
            }
            xSemaphoreGive(mtx_OvenState);
        }
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100)); // 10Hz safety check
    }
}

// --- MCP9600 Interface ---
// --- Legacy Sensor Drivers Removed (Using direct I2C) ---
// MCP9600 is handled via read_mcp9600_temp()
// DS18B20 is removed/disabled as per request (AUX not connected)

void vSensorPollerTask(void *pvParameters) {
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    // Initial Setup
    // Raw I2C used, no specific setup_sensors() needed.
    // I2C bus init happens in main().
    
    uint32_t log_counter = 0;

    for (;;) {
        // 1. Read MCP9600 (I2C) - Fast (approx 5-10ms)
        if (xSemaphoreTake(mtx_I2C, pdMS_TO_TICKS(20)) == pdTRUE) {
            float t = read_mcp9600_temp(I2C_ADDR_MCP9600_T1);
            
            // Check for error (e.g. -999.0f)
            if (t > -100.0f) {
                 if (xSemaphoreTake(mtx_OvenState, pdMS_TO_TICKS(5)) == pdTRUE) {
                    ovenState.current_temp_t1 = t;
                    xSemaphoreGive(mtx_OvenState);
                 }
                 if (log_counter++ % 10 == 0) { // Log every 2s
                     printf("[Sensors] T1: %.2f C\n", t);
                 }
            } else {
                 printf("[Sensors] MCP9600 Read Failed\n");
            }
            xSemaphoreGive(mtx_I2C);
        }
        
        // Loop at 5Hz (200ms)
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(200));
    }
}

void vPIDLoopTask(void *pvParameters) {
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    float integral = 0.0f;
    float last_error = 0.0f;
    
    // PID Config (Loaded from system.json)
    // If config not loaded (defaults in load_system_config should handle this), 
    // we use safe fallbacks.
    float kp1 = (sysConfig.pid_ssr1_kp > 0) ? sysConfig.pid_ssr1_kp : 4.0f;
    float ki1 = (sysConfig.pid_ssr1_ki > 0) ? sysConfig.pid_ssr1_ki : 0.02f;
    float kd1 = (sysConfig.pid_ssr1_kd > 0) ? sysConfig.pid_ssr1_kd : 50.0f; 

    // SSR2 Params
    float kp2 = (sysConfig.pid_ssr2_kp > 0) ? sysConfig.pid_ssr2_kp : 4.0f;
    float ki2 = (sysConfig.pid_ssr2_ki > 0) ? sysConfig.pid_ssr2_ki : 0.02f;
    float kd2 = (sysConfig.pid_ssr2_kd > 0) ? sysConfig.pid_ssr2_kd : 50.0f;
    
    // SSR2 State
    float integral2 = 0.0f;
    float last_error2 = 0.0f;
    
    for (;;) {
        float input = 0;
        float setpoint = 0;
        OvenStateEnum state = STATE_IDLE;
        
        if (xSemaphoreTake(mtx_OvenState, pdMS_TO_TICKS(10)) == pdTRUE) {
            input = ovenState.current_temp_t1;
            setpoint = ovenState.target_temp;
            state = ovenState.state;
            xSemaphoreGive(mtx_OvenState);
        }
        
        float output1 = 0;
        float output2 = 0;
        
        if (state == STATE_RUNNING || state == STATE_PRE_CHECK || state == STATE_MANUAL) {
            float error = setpoint - input;
            
            // --- PID 1 ---
            integral += error;
            if (integral > 2500.0f) integral = 2500.0f;
            if (integral < -2500.0f) integral = -2500.0f;
            
            float derivative = error - last_error;
            last_error = error;
            
            output1 = (kp1 * error) + (ki1 * integral) + (kd1 * derivative);
            
            if (output1 > 100.0f) output1 = 100.0f;
            if (output1 < 0.0f) output1 = 0.0f;
            
            // --- PID 2 (If Present) ---
            if (sysConfig.ssr2_is_present) {
                 integral2 += error;
                 if (integral2 > 2500.0f) integral2 = 2500.0f;
                 if (integral2 < -2500.0f) integral2 = -2500.0f;
                 
                 float derivative2 = error - last_error2;
                 last_error2 = error;
                 
                 output2 = (kp2 * error) + (ki2 * integral2) + (kd2 * derivative2);
                 
                 if (output2 > 100.0f) output2 = 100.0f;
                 if (output2 < 0.0f) output2 = 0.0f;
            }

            // CSV Log: Using simplified log for now or extend it
            // printf("[PID],%d,SP=%.2f,T=%.2f,OUT1=%.2f,OUT2=%.2f\n", xTaskGetTickCount(), setpoint, input, output1, output2);
            
        } else {
             output1 = 0; integral = 0; last_error = 0;
             output2 = 0; integral2 = 0; last_error2 = 0;
        }
        
        // Update Output
        if (xSemaphoreTake(mtx_OvenState, pdMS_TO_TICKS(10)) == pdTRUE) {
            ovenState.power_output_1 = output1;
            ovenState.power_output_2 = output2;
            xSemaphoreGive(mtx_OvenState);
        }
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(200)); // 5Hz Control Loop
    }
}

// --- Duplicate Removed ---

// --- HW Config for FatFs Library ---
// See hw_config.h

// SPI0 Configuration
static spi_t spi0_obj = {
    .hw_inst = spi0,
    .miso_gpio = GPIO_SPI_MISO, 
    .mosi_gpio = GPIO_SPI_MOSI, 
    .sck_gpio = GPIO_SPI_SCK,  
    .baud_rate = 10 * 1000 * 1000, 
    //.DMA_IRQ_num = DMA_IRQ_0 // Handled by library init?
};

// SD Card Configuration
static sd_card_t sd_card_obj = {
    .pcName = "0:",
    .spi = &spi0_obj,
    .ss_gpio = 22, // GPIO_SD_CS
    .use_card_detect = false,
    .card_detect_gpio = 0,
    .card_detected_true = 0,
    .m_Status = STA_NOINIT
};

// Implement required functions
extern "C" size_t spi_get_num() { return 1; }
extern "C" spi_t *spi_get_by_num(size_t num) { return &spi0_obj; }
extern "C" size_t sd_get_num() { return 1; }
extern "C" sd_card_t *sd_get_by_num(size_t num) { return &sd_card_obj; }

// --- System Configuration ---
// Defined at top

// Global FS Objects
FATFS sdCardFS;
bool sd_mounted = false;

// --- Helper Functions ---

// Load System Config
void load_system_config() {
    FIL file;
    if (f_open(&file, "/config/system.json", FA_READ) == FR_OK) {
        UINT read_bytes;
        char *buffer = (char*)malloc(f_size(&file) + 1);
        if (buffer) {
            f_read(&file, buffer, f_size(&file), &read_bytes);
            buffer[read_bytes] = 0;
            f_close(&file);
            
            cJSON *json = cJSON_Parse(buffer);
            if (json) {
                // Parse Hardware
                cJSON *hw = cJSON_GetObjectItem(json, "hardware");
                if (hw) {
                    cJSON *item = cJSON_GetObjectItem(hw, "enable_sensor2_check");
                    if (item) sysConfig.enable_sensor2_check = cJSON_IsTrue(item);
                    
                    item = cJSON_GetObjectItem(hw, "screen_orientation");
                    if (item) sysConfig.screen_orientation = item->valueint;

                    item = cJSON_GetObjectItem(hw, "ssr2_is_present");
                    if (item) sysConfig.ssr2_is_present = item->valueint;
                }
                
                // Parse PID
                cJSON *pid = cJSON_GetObjectItem(json, "pid_params");
                if (pid) {
                    cJSON *s1 = cJSON_GetObjectItem(pid, "ssr1");
                    if (s1) {
                        sysConfig.pid_ssr1_kp = cJSON_GetObjectItem(s1, "kp")->valuedouble;
                        sysConfig.pid_ssr1_ki = cJSON_GetObjectItem(s1, "ki")->valuedouble;
                        sysConfig.pid_ssr1_kd = cJSON_GetObjectItem(s1, "kd")->valuedouble;
                    }
                    cJSON *s2 = cJSON_GetObjectItem(pid, "ssr2");
                    if (s2) {
                        sysConfig.pid_ssr2_kp = cJSON_GetObjectItem(s2, "kp")->valuedouble;
                        sysConfig.pid_ssr2_ki = cJSON_GetObjectItem(s2, "ki")->valuedouble;
                        sysConfig.pid_ssr2_kd = cJSON_GetObjectItem(s2, "kd")->valuedouble;
                    }
                }
                
                // Calibration
                cJSON *cal = cJSON_GetObjectItem(json, "calibration");
                if (cal) {
                    sysConfig.t1_offset = cJSON_GetObjectItem(cal, "t1_offset")->valuedouble;
                }

                cJSON_Delete(json);
                printf("Config Loaded!\n");
            }
            free(buffer);
        } else {
             f_close(&file);
        }
    } else {
        printf("Config File Not Found!\n");
    }
}

void save_system_config() {
    // Manual JSON serialization since cJSON_Print/Create is missing/stripped
    char* buffer = (char*)malloc(1024); // Shared buffer
    if (!buffer) return;

    // We can just format the string directly.
    // Note: Floats formatting might need care
    int len = snprintf(buffer, 1024, 
        "{\n"
        "  \"hardware\": {\n"
        "    \"enable_sensor2_check\": %s,\n"
        "    \"screen_orientation\": %d,\n"
        "    \"ssr2_is_present\": %d\n"
        "  },\n"
        "  \"pid_params\": {\n"
        "    \"ssr1\": {\n"
        "      \"kp\": %.2f,\n"
        "      \"ki\": %.4f,\n"
        "      \"kd\": %.2f\n"
        "    },\n"
        "    \"ssr2\": {\n"
        "      \"kp\": %.2f,\n"
        "      \"ki\": %.4f,\n"
        "      \"kd\": %.2f\n"
        "    }\n"
        "  },\n"
        "  \"calibration\": {\n"
        "    \"t1_offset\": %.2f\n"
        "  }\n"
        "}",
        sysConfig.enable_sensor2_check ? "true" : "false",
        sysConfig.screen_orientation,
        sysConfig.ssr2_is_present,
        sysConfig.pid_ssr1_kp, sysConfig.pid_ssr1_ki, sysConfig.pid_ssr1_kd,
        sysConfig.pid_ssr2_kp, sysConfig.pid_ssr2_ki, sysConfig.pid_ssr2_kd,
        sysConfig.t1_offset
    );

    if (len > 0) {
        FIL file;
        if (f_open(&file, "/config/system.json", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
            UINT written;
            f_write(&file, buffer, len, &written);
            f_close(&file);
            printf("Config Saved! (%d bytes)\n", written);
        } else {
            printf("Failed to Open Config for Writing!\n");
        }
    }
    free(buffer);
}

// Load Profile
void load_profile(const char* path) {
    FIL file;
    if (f_open(&file, path, FA_READ) == FR_OK) {
        UINT read_bytes;
        char *buffer = (char*)malloc(f_size(&file) + 1);
        if (buffer) {
            f_read(&file, buffer, f_size(&file), &read_bytes);
            buffer[read_bytes] = 0;
            f_close(&file);
            
            cJSON *json = cJSON_Parse(buffer);
            if (json) {
                // Meta
                cJSON *meta = cJSON_GetObjectItem(json, "meta");
                if (meta) {
                    cJSON *nm = cJSON_GetObjectItem(meta, "name");
                    if (nm && nm->valuestring) {
                        strncpy(currentProfile.name, nm->valuestring, 31);
                        currentProfile.name[31] = 0; // Ensure null term
                        printf("[Profile] Name Parsed: '%s'\n", currentProfile.name);
                    } else {
                        printf("[Profile] Name NOT found in JSON\n");
                        strncpy(currentProfile.name, "Unknown", 31);
                    }
                } else {
                    printf("[Profile] Meta block not found\n");
                    strncpy(currentProfile.name, "No Meta", 31);
                }
                
                // Segments
                cJSON *segs = cJSON_GetObjectItem(json, "segments");
                int count = cJSON_GetArraySize(segs);
                if (count > MAX_PROFILE_SEGMENTS) count = MAX_PROFILE_SEGMENTS;
                
                currentProfile.segment_count = count;
                float last_temp = 25.0f; // Assumed start
                
                for (int i=0; i<count; i++) {
                    cJSON *s = cJSON_GetArrayItem(segs, i);
                    cJSON *type = cJSON_GetObjectItem(s, "type");
                    
                    if (strcmp(type->valuestring, "ramp") == 0) currentProfile.segments[i].type = SEG_RAMP;
                    else if (strcmp(type->valuestring, "hold") == 0) currentProfile.segments[i].type = SEG_HOLD;
                    else currentProfile.segments[i].type = SEG_STEP;
                    
                    // Target Temp
                    if (cJSON_GetObjectItem(s, "end_temp") != NULL) 
                        currentProfile.segments[i].target_temp = cJSON_GetObjectItem(s, "end_temp")->valuedouble;
                    else if (cJSON_GetObjectItem(s, "temp") != NULL)
                        currentProfile.segments[i].target_temp = cJSON_GetObjectItem(s, "temp")->valuedouble;
                        
                    // Duration or Slope
                    if (cJSON_GetObjectItem(s, "duration") != NULL)
                         currentProfile.segments[i].duration = cJSON_GetObjectItem(s, "duration")->valueint;
                    else if (cJSON_GetObjectItem(s, "duration_s") != NULL)
                         currentProfile.segments[i].duration = cJSON_GetObjectItem(s, "duration_s")->valueint;
                    else if (cJSON_GetObjectItem(s, "slope") != NULL) {
                        // Calculate Duration from Slope
                        float slope = cJSON_GetObjectItem(s, "slope")->valuedouble;
                        currentProfile.segments[i].slope = slope;
                        if (slope != 0) {
                            float diff = fabsf(currentProfile.segments[i].target_temp - last_temp);
                            currentProfile.segments[i].duration = (uint32_t)(diff / fabsf(slope));
                        } else {
                            currentProfile.segments[i].duration = 0;
                        }
                    }
                    
                    last_temp = currentProfile.segments[i].target_temp;
                }
                
                cJSON_Delete(json);
                printf("Profile Loaded: %s\n", currentProfile.name);
            }
            free(buffer);
        } else {
            f_close(&file);
        }
    }
}

// --- Hardcoded Profile (SAC305 approx) ---
// ReflowProfile currentProfile; // Moved to Global Objects

void init_test_profile() {
    // Basic Fallback if load fails
    snprintf(currentProfile.name, 32, "SAC305 Default");
    currentProfile.segment_count = 5;
    
    // 1. Preheat (Ramp to 150C in 90s -> ~1.6C/s)
    currentProfile.segments[0].type = SEG_RAMP;
    currentProfile.segments[0].target_temp = 150.0f;
    currentProfile.segments[0].duration = 90; 
    currentProfile.segments[0].slope = 1.5f; 
    snprintf(currentProfile.segments[0].note, 16, "Preheat");

    // ... (Rest of default profile)
    currentProfile.segments[1].type = SEG_HOLD;
    currentProfile.segments[1].target_temp = 150.0f;
    currentProfile.segments[1].duration = 60;
    snprintf(currentProfile.segments[1].note, 16, "Soak");

    currentProfile.segments[2].type = SEG_RAMP;
    currentProfile.segments[2].target_temp = 245.0f;
    currentProfile.segments[2].duration = 60; 
    snprintf(currentProfile.segments[2].note, 16, "Ramp Up");

    currentProfile.segments[3].type = SEG_HOLD;
    currentProfile.segments[3].target_temp = 245.0f;
    currentProfile.segments[3].duration = 20;
    snprintf(currentProfile.segments[3].note, 16, "Reflow");
    
    currentProfile.segments[4].type = SEG_RAMP;
    currentProfile.segments[4].target_temp = 50.0f;
    currentProfile.segments[4].duration = 60;
    snprintf(currentProfile.segments[4].note, 16, "Cooling");
}

void vAppLogicTask(void *pvParameters) {
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    // Default Init
    init_test_profile(); 
    
    // Wait for system stablization
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Auto-transition INIT -> IDLE using mutex
    if (xSemaphoreTake(mtx_OvenState, pdMS_TO_TICKS(100)) == pdTRUE) {
        ovenState.state = STATE_IDLE;
        xSemaphoreGive(mtx_OvenState);
    }
    
    // --- Initial File System Mount ---
    // Try to mount SD card and load real configs
    if (f_mount(&sdCardFS, "", 1) == FR_OK) {
        printf("SD Card Mounted.\n");
        sd_mounted = true;
        load_system_config();
        
        // Try loading profile
        ReflowProfile backup = currentProfile; // Save default
        // Clear current for clean load? 
        // load_profile("/profiles/sac305.json");
        // If load fails (name empty?), revert?
        // simple check:
        // if (currentProfile.segment_count == 0) currentProfile = backup;
    } else {
        printf("SD Mount Failed - Using Defaults\n");
    }

    // Auto-Start (Removed for Manual Button Control)
    bool test_started = false;
    uint32_t boot_time = to_ms_since_boot(get_absolute_time());

    for (;;) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        
        // --- Input Handling ---
        InputEvent evt;
        if (xQueueReceive(q_InputEvents, &evt, 0) == pdTRUE) {
            printf("Input Event: %d\n", evt.type);

            if (xSemaphoreTake(mtx_OvenState, pdMS_TO_TICKS(50)) == pdTRUE) {
                // UI Navigation Logic via UI Manager
                
                // Protect LVGL Calls
                if (xSemaphoreTake(mtx_LVGL, pdMS_TO_TICKS(100)) == pdTRUE) {
                    UIScreenEnum prev_screen = uiCtx.current_screen;
                    ui_process_input(evt);
                    
                    // If screen didn't change, check for Contextual Actions
                    if (uiCtx.current_screen == prev_screen) {
                       // ... (Contextual actions)
                    }
                    xSemaphoreGive(mtx_LVGL); // Release UI Lock
                    
                    // Logic that DOES NOT need LVGL mutex but needs OvenState mutex
                    if (uiCtx.current_screen == UI_SCREEN_DASHBOARD) {
                        // Dashboard Controls
                        if (evt.type == EVT_BTN1_PRESS) { // START / STOP
                            // ... (Logic)
                            if (ovenState.state == STATE_IDLE || ovenState.state == STATE_COOLDOWN || ovenState.state == STATE_INIT) {
                                if (currentProfile.segment_count > 0) {
                                    ovenState.state = STATE_PRE_CHECK;
                                    ovenState.profile_start_time = millis();
                                    ovenState.current_segment_index = 0;
                                    printf("CMD: Start Profile\n");
                                }
                            } else if (ovenState.state == STATE_RUNNING || ovenState.state == STATE_PRE_CHECK) {
                                ovenState.state = STATE_COOLDOWN;
                                printf("CMD: Stop Profile\n");
                            } else if (ovenState.state == STATE_FAULT) {
                                ovenState.state = STATE_IDLE;
                                ovenState.fault_active = false;
                                printf("CMD: Ack Fault\n");
                            }
                        }
                    }
                     else if (uiCtx.current_screen == UI_SCREEN_MANUAL) {
                        // Manual Mode Logic (Toggle Heater)
                        if (evt.type == EVT_BTN1_PRESS) {
                            if (ovenState.state == STATE_IDLE) {
                                 printf("Manual Toggle\n");
                            }
                        }
                    } 
                }
                xSemaphoreGive(mtx_OvenState);
            }
        }
        
        /* Auto-start removed to test buttons */
        /*
        if (!test_started && (now - boot_time > 5000)) {
           
        }
        */

        // State machine logic
        if (xSemaphoreTake(mtx_OvenState, pdMS_TO_TICKS(50)) == pdTRUE) {
            OvenStateEnum currentState = ovenState.state;
            
            switch (currentState) {
                case STATE_INIT:
                    break;
                case STATE_IDLE:
                    ovenState.power_output_1 = 0;
                    ovenState.target_temp = 0;
                    break;
                case STATE_PRE_CHECK:
                    // Simple Safety Check before starting
                    if (ovenState.current_temp_t1 > 0 && ovenState.current_temp_t1 < 300) {
                         ovenState.state = STATE_RUNNING;
                         ovenState.profile_start_time = millis(); // Reset start time
                         printf("Pre-Check OK -> RUNNING\n");
                    } else {
                         ovenState.state = STATE_FAULT;
                         printf("Pre-Check FAILED (T1=%.1f)\n", ovenState.current_temp_t1);
                    }
                    break;
                case STATE_MANUAL:
                    // Do nothing, let PID run
                    break;
                case STATE_RUNNING: {
                    uint32_t elapsed_total_sec = (millis() - ovenState.profile_start_time) / 1000;
                    uint32_t seg_accum_time = 0;
                    int active_seg = -1;
                    
                    // Find current segment based on elapsed time
                    for (int i=0; i < currentProfile.segment_count; i++) {
                        if (elapsed_total_sec < (seg_accum_time + currentProfile.segments[i].duration)) {
                            active_seg = i;
                            // Calculate Target Temp interpolation
                            uint32_t seg_local_time = elapsed_total_sec - seg_accum_time;
                            ProfileSegment *s = &currentProfile.segments[i];
                            
                            if (s->type == SEG_HOLD) {
                                ovenState.target_temp = s->target_temp;
                            } 
                            else if (s->type == SEG_RAMP) {
                                // interpolate from previous segment end temp
                                float start_temp = 25.0f; // Default room temp
                                if (i > 0) start_temp = currentProfile.segments[i-1].target_temp;
                                
                                float progress = (float)seg_local_time / (float)s->duration;
                                ovenState.target_temp = start_temp + (s->target_temp - start_temp) * progress;
                            } else {
                                ovenState.target_temp = s->target_temp; // Step
                            }
                            
                            ovenState.current_segment_index = i;
                            break;
                        }
                        seg_accum_time += currentProfile.segments[i].duration;
                    }
                    
                    if (active_seg == -1) {
                        // Profile Finished
                        ovenState.state = STATE_COOLDOWN;
                    }
                    break;
                }
                case STATE_COOLDOWN:
                    ovenState.power_output_1 = 0;
                    ovenState.target_temp = 0;
                    if (ovenState.current_temp_t1 < 50.0f && ovenState.current_temp_t1 > 0) {
                        ovenState.state = STATE_IDLE;
                    }
                    // Timeout safety?
                    break;
                case STATE_FAULT:
                    ovenState.power_output_1 = 0;
                    break;
                default:
                    break;
            }
            xSemaphoreGive(mtx_OvenState);
        }
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100)); // 10Hz logic
    }
}

// --- Task Functions (Core 1) ---

// --- UI Task ---
void vUITask(void *pvParameters) {
    // Affinity to Core 1 is set in main
    printf("[UI] Starting UI Task on Core %d\n", get_core_num());
    
    // UI Init (LVGL + Drivers + Screens)
    // Note: If other tasks try to access UI during init they might crash, 
    // but they shouldn't until we start scheduler.
    // However, xSemaphoreTake(mtx_LVGL) is needed if vAppLogicTask runs immediately.
    
    // We can't take mutex before creating it, but task creation is after mutex gen.
    if (xSemaphoreTake(mtx_LVGL, pdMS_TO_TICKS(1000))) {
         ui_init(); 
         xSemaphoreGive(mtx_LVGL);
    } else {
        printf("[UI] MUTEX ERROR during init\n");
    }
    
    for (;;) {
        // Update State from global ovenState
        OvenState localState;
        bool state_valid = false;
        
        if (xSemaphoreTake(mtx_OvenState, pdMS_TO_TICKS(10)) == pdTRUE) {
            localState = ovenState;
            state_valid = true;
            xSemaphoreGive(mtx_OvenState);
        }
        
        // Take LVGL Mutex for Rendering Cycle
        if (xSemaphoreTake(mtx_LVGL, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (state_valid) {
                 ui_update_state(&localState);
            }
            ui_tick(); // Calls lv_tick_inc and lv_timer_handler
            xSemaphoreGive(mtx_LVGL);
        }
        
        // Delay to yield (LVGL needs fairly frequent updates)
        vTaskDelay(pdMS_TO_TICKS(5)); 
    }
}

// --- SD Card Test ---
void test_sd_card() {
    printf("\n[SD] Initializing SD Card...\n");
    
    // Mount (force mount to check hardware)
    FRESULT fr = f_mount(&sdCardFS, "0:", 1);
    if (fr != FR_OK) {
        printf("[SD] Mount Failed! Error: %d\n", fr);
        if (fr == FR_NOT_READY) printf("[SD] Check connections/card insertion.\n");
        return;
    }
    printf("[SD] Mount Successful.\n");
    sd_mounted = true;
    
    // Write Test
    printf("[SD] Writing 'test.txt'...\n");
    FIL fil;
    fr = f_open(&fil, "test.txt", FA_WRITE | FA_CREATE_ALWAYS);
    if (fr == FR_OK) {
        f_printf(&fil, "MTR Reflow Oven SD Test. Time: %d ms\n", millis());
        f_close(&fil);
        printf("[SD] Write OK.\n");
    } else {
        printf("[SD] Write Failed! Error: %d\n", fr);
    }
    
    // Read Test
    printf("[SD] Reading 'test.txt'...\n");
    char line[128];
    fr = f_open(&fil, "test.txt", FA_READ);
    if (fr == FR_OK) {
        if (f_gets(line, sizeof(line), &fil)) {
            printf("[SD] Read Content: %s", line);
        }
        f_close(&fil);
    } else {
        printf("[SD] Read Failed! Error: %d\n", fr);
    }
}

// --- Main ---

int main() {
    stdio_init_all();
    
    // Wait for USB serial connection
    sleep_ms(2000);
    
    printf("\n\n");
    printf("========================================\n");
    printf("   MTR REFLOW OVEN - STARTING UP\n");
    printf("========================================\n");
    printf("\n");

    // --- HW Init ---
    // Initialize I2C
    printf("[I2C] Initializing I2C0 on GPIO %d (SDA) / %d (SCL)...\n", GPIO_I2C_SDA, GPIO_I2C_SCL);
    i2c_init(I2C_PORT, 100 * 1000); // Lowered to 100kHz for reliability
    gpio_set_function(GPIO_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(GPIO_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(GPIO_I2C_SDA);
    gpio_pull_up(GPIO_I2C_SCL);
    printf("[I2C] I2C initialized at 100kHz\n");
    
    // === I2C BUS SCAN ===
    printf("\n[I2C] Scanning I2C bus...\n");
    printf("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
    for (int addr_base = 0; addr_base < 128; addr_base += 16) {
        printf("%02X: ", addr_base);
        for (int addr_offset = 0; addr_offset < 16; addr_offset++) {
            int addr = addr_base + addr_offset;
            if (addr < 0x08 || addr > 0x77) {
                printf("   ");
            } else {
                uint8_t rxdata;
                int ret = i2c_read_blocking(I2C_PORT, addr, &rxdata, 1, false);
                if (ret >= 0) {
                    printf("%02X ", addr);
                } else {
                    printf("-- ");
                }
            }
        }
        printf("\n");
    }
    printf("[I2C] Scan complete\n\n");
    
    // === GPIO DEBUG TEST ===
    printf("[GPIO] Initializing buttons and encoder...\n");
    gpio_init(GPIO_BTN1); gpio_set_dir(GPIO_BTN1, GPIO_IN); gpio_pull_up(GPIO_BTN1);
    gpio_init(GPIO_BTN2); gpio_set_dir(GPIO_BTN2, GPIO_IN); gpio_pull_up(GPIO_BTN2);

    gpio_init(GPIO_ROT_BTN); gpio_set_dir(GPIO_ROT_BTN, GPIO_IN); gpio_pull_up(GPIO_ROT_BTN);
    gpio_init(GPIO_ROT_DT);  gpio_set_dir(GPIO_ROT_DT,  GPIO_IN); gpio_pull_up(GPIO_ROT_DT);
    gpio_init(GPIO_ROT_CLK); gpio_set_dir(GPIO_ROT_CLK, GPIO_IN); gpio_pull_up(GPIO_ROT_CLK);
    
    // Read initial button states
    printf("[GPIO] Button states: BTN1=%d BTN2=%d ROT_BTN=%d\n",
           gpio_get(GPIO_BTN1), gpio_get(GPIO_BTN2), gpio_get(GPIO_ROT_BTN));
    printf("[GPIO] Encoder: CLK=%d DT=%d\n", gpio_get(GPIO_ROT_CLK), gpio_get(GPIO_ROT_DT));
    
    // === BUZZER TEST ===
    printf("\n[BUZZER] Testing buzzer on GPIO %d...\n", GPIO_BUZZER);
    gpio_init(GPIO_BUZZER);
    gpio_set_dir(GPIO_BUZZER, GPIO_OUT);
    // === BUZZER TEST (500Hz confirmed loudest) ===
    printf("\n[BUZZER] Testing buzzer on GPIO %d (500Hz)...\n", GPIO_BUZZER);
    gpio_init(GPIO_BUZZER);
    gpio_set_dir(GPIO_BUZZER, GPIO_OUT);
    gpio_set_drive_strength(GPIO_BUZZER, GPIO_DRIVE_STRENGTH_12MA);
    
    // Play "OK" pattern: 3 short beeps
    play_tone(500, 100); sleep_ms(100);
    play_tone(500, 100); sleep_ms(100);
    play_tone(500, 100);
    printf("[BUZZER] Startup tones complete\n");
    
    // === SD CARD TEST ===
    test_sd_card();
    
    // === SSR TEST ===
    printf("\n[SSR] Testing SSR outputs on GPIO %d and %d...\n", GPIO_HEAT1, GPIO_HEAT2);
    gpio_init(GPIO_HEAT1);
    gpio_init(GPIO_HEAT2);
    gpio_set_dir(GPIO_HEAT1, GPIO_OUT);
    gpio_set_dir(GPIO_HEAT2, GPIO_OUT);
    gpio_put(GPIO_HEAT1, 0);
    gpio_put(GPIO_HEAT2, 0);
    printf("[SSR] SSRs initialized (OFF)\n");
    
    // LED Test removed (WS2812B handled by vFeedbackTask)


    // SPI Init / LCD Init removed (handled by hagl_init in vUITask)
    printf("Skipping Raw SPI Test.\n");

    
    // SPI Init handled by hagl_init() later
    // Interrupts handled by vInputTask (Polling)
    
    // --- FreeRTOS Objects ---
    mtx_SPI0 = xSemaphoreCreateMutex();
    mtx_I2C = xSemaphoreCreateMutex();
    mtx_OvenState = xSemaphoreCreateMutex();
    mtx_LVGL = xSemaphoreCreateMutex();
    
    q_SensorData = xQueueCreate(5, sizeof(SensorData));
    q_InputEvents = xQueueCreate(10, sizeof(InputEvent));
    
    // Default State Init
    ovenState.state = STATE_INIT;
    ovenState.t2_connected = false;
    ovenState.fault_active = false;
    
    // --- Initial File System Mount ---
    // Note: SD Card shares SPI with TFT. Need proper CS management?
    // FatFs usually handles low-level SPI if configured correctly.
    // For now, we assume standard mounting logic.
    /*
    if (f_mount(&fs, "", 1) == FR_OK) {
        sd_mounted = true;
        load_system_config();
        // pre-load default profile?
        load_profile("/profiles/sac305.json");
    } else {
        printf("SD Mount Failed!\n");
        // Fallback defaults
        init_test_profile();
    }
    */
    init_test_profile(); // Defaulting for now until SD HW verified

    // --- Interrupts (DISABLED - Using vInputTask Polling) ---
    // gpio_set_irq_enabled_with_callback(GPIO_BTN1, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    // gpio_set_irq_enabled(GPIO_BTN2, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);

    // gpio_set_irq_enabled(GPIO_ROT_BTN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    // gpio_set_irq_enabled(GPIO_ROT_DT, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    // gpio_set_irq_enabled(GPIO_T1_ALT1, GPIO_IRQ_EDGE_FALL, true);
    
    // --- Create Tasks ---
    
    // Core 0 Tasks
    xTaskCreate(vAppLogicTask, "AppLogic", 2048, NULL, 3, &hAppLogicTask);
    xTaskCreate(vSensorPollerTask, "Sensors", 1024, NULL, 2, NULL); 
    // xTaskCreate(vAuxiliaryTask, "AuxSensors", 1024, NULL, 1, NULL); // DISABLED - DS18B20 not connected
    xTaskCreate(vPIDLoopTask, "PID", 1024, NULL, 2, &hPIDTask);
    xTaskCreate(vAlertHandlingTask, "Alerts", 512, NULL, 5, &hAlertTask);
    
    // Input Task (Polling)
    xTaskCreate(vInputTask, "Input", 1024, NULL, 3, NULL);
    
    // Output Tasks
    xTaskCreate(vSSRControlTask, "SSR_PWM", 512, NULL, 4, NULL);
    xTaskCreate(vFeedbackTask, "Feedback", 512, NULL, 2, NULL);

    // Core 1 (UI)
    // Note: Display is not working yet, but task structure is preserved.
    xTaskCreate(vUITask, "UI_Manager", 2048, NULL, 2, &hTFTDebugTask);
    
    // Pinning example (if supported):
    // vTaskCoreAffinitySet(hTFTDebugTask, (1 << 1)); // Pin to Core 1
    // vTaskCoreAffinitySet(hAlertTask, (1 << 0));    // Pin to Core 0
    
    // Initialize TFT before scheduler to ensure hardware is ready ? 
    // Or protect with Mutex. TFT_eSPI init isn't thread safe usually.
    // Better to init here.
    // tft.init();
    // tft.setRotation(1); // Landscape
    // tft.fillScreen(TFT_BLUE);
    // tft.setCursor(10, 10);
    // tft.println("Booting...");

    // Start scheduler (never returns)
    printf("Starting Scheduler...\n");
    vTaskStartScheduler();

    while (true) {
        // Should not reach here
        tight_loop_contents();
    }
}
