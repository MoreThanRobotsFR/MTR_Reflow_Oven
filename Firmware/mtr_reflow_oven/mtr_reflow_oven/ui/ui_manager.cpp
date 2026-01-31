#include "ui_manager.h"
#include "ui_screens.h"
#include "ui_shared.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <stdio.h>

// --- Hardware Config ---
#define ST7796_DC   25
#define ST7796_CS   21
#define ST7796_CLK  18
#define ST7796_MOSI 19
#define ST7796_RST  24
#define ST7796_BL   23

extern SemaphoreHandle_t mtx_SPI0;
extern UIContext uiCtx;

// --- Display Driver ---

static void st7796_write_cmd(uint8_t cmd) {
    gpio_put(ST7796_DC, 0); // Command
    gpio_put(ST7796_CS, 0);
    spi_write_blocking(spi0, &cmd, 1);
    gpio_put(ST7796_CS, 1);
}

static void st7796_write_data(const uint8_t *data, size_t len) {
    if (len == 0) return;
    gpio_put(ST7796_DC, 1); // Data
    gpio_put(ST7796_CS, 0);
    spi_write_blocking(spi0, data, len);
    gpio_put(ST7796_CS, 1);
}

static void init_st7796() {
    // SPI Init (Ensure 40MHz or reasonable speed)
    spi_init(spi0, 40000000);
    gpio_set_function(ST7796_CLK, GPIO_FUNC_SPI);
    gpio_set_function(ST7796_MOSI, GPIO_FUNC_SPI);
    
    // GPIO Init
    gpio_init(ST7796_DC); gpio_set_dir(ST7796_DC, GPIO_OUT);
    gpio_init(ST7796_CS); gpio_set_dir(ST7796_CS, GPIO_OUT);
    gpio_init(ST7796_RST); gpio_set_dir(ST7796_RST, GPIO_OUT);
    gpio_init(ST7796_BL); gpio_set_dir(ST7796_BL, GPIO_OUT);
    
    gpio_put(ST7796_CS, 1);
    gpio_put(ST7796_BL, 1); // Backlight ON
    
    // Reset
    gpio_put(ST7796_RST, 0); sleep_ms(100);
    gpio_put(ST7796_RST, 1); sleep_ms(100);
    
    // Init Sequence (Standard ST7796)
    st7796_write_cmd(0x01); sleep_ms(150); // SWRESET
    st7796_write_cmd(0x36); // MADCTL
    uint8_t d = 0x28; // BGR | MV (Landscape)
    st7796_write_data(&d, 1);
    
    st7796_write_cmd(0x3A); // COLMOD
    d = 0x55; // 16-bit
    st7796_write_data(&d, 1);
    
    st7796_write_cmd(0x11); sleep_ms(50); // SLPOUT
    st7796_write_cmd(0x29); sleep_ms(10); // DISPON
}

static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
    if (xSemaphoreTake(mtx_SPI0, pdMS_TO_TICKS(100)) == pdTRUE) {
        uint16_t x1 = area->x1;
        uint16_t x2 = area->x2;
        uint16_t y1 = area->y1;
        uint16_t y2 = area->y2;
        
        uint8_t data[4];
        
        st7796_write_cmd(0x2A); // CASET
        data[0] = x1 >> 8; data[1] = x1 & 0xFF;
        data[2] = x2 >> 8; data[3] = x2 & 0xFF;
        st7796_write_data(data, 4);
        
        st7796_write_cmd(0x2B); // RASET
        data[0] = y1 >> 8; data[1] = y1 & 0xFF;
        data[2] = y2 >> 8; data[3] = y2 & 0xFF;
        st7796_write_data(data, 4);
        
        st7796_write_cmd(0x2C); // RAMWR
        
        uint32_t size = lv_area_get_width(area) * lv_area_get_height(area) * 2;
        st7796_write_data(px_map, size);
        
        xSemaphoreGive(mtx_SPI0);
    }
    lv_display_flush_ready(disp);
}


// --- UI Manager ---
void ui_init(void) {
    // 1. Init LVGL
    lv_init();
    
    // 2. Init Drivers
    if (xSemaphoreTake(mtx_SPI0, pdMS_TO_TICKS(1000))) {
        init_st7796();
        xSemaphoreGive(mtx_SPI0);
    } else {
        printf("[UI] Failed to init Display (Mutex Timeout)\n");
    }
    
    // 3. Create Display Object
    static lv_color_t buf1[480 * 20]; // 10% buffer
    lv_display_t * disp = lv_display_create(480, 320);
    lv_display_set_flush_cb(disp, disp_flush);
    lv_display_set_buffers(disp, buf1, NULL, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    // 4. Init Styles & Screens
    ui_styles_init();
    ui_create_menu();
    ui_create_dashboard();
    ui_create_manual();
    ui_create_settings();
    ui_create_profile();
    
    // 5. Default Screen
    lv_screen_load(scr_menu);
    uiCtx.current_screen = UI_SCREEN_MAIN_MENU;
}

void ui_process_input(InputEvent evt) {
    // Global Navigation overrides could go here
    
    // Dispatch to current screen
    switch(uiCtx.current_screen) {
        case UI_SCREEN_MAIN_MENU: ui_screen_menu_input(evt); break;
        case UI_SCREEN_DASHBOARD: ui_screen_dashboard_input(evt); break;
        case UI_SCREEN_MANUAL:    ui_screen_manual_input(evt); break;
        case UI_SCREEN_SETTINGS:  ui_screen_settings_input(evt); break;
        case UI_SCREEN_PROFILE_SELECT: ui_screen_profile_input(evt); break;
        // ...
        default: break;
    }
    
    // Handle Screen Switching requests
    if (uiCtx.full_redraw) {
        switch(uiCtx.current_screen) {
            case UI_SCREEN_MAIN_MENU: lv_screen_load(scr_menu); break;
            case UI_SCREEN_DASHBOARD: lv_screen_load(scr_dashboard); break;
            case UI_SCREEN_MANUAL:    lv_screen_load(scr_manual); break;
            case UI_SCREEN_SETTINGS:  lv_screen_load(scr_settings); break;
            case UI_SCREEN_PROFILE_SELECT: lv_screen_load(scr_profile); break;
            default: break;
        }
        uiCtx.full_redraw = false;
    }
}

void ui_update_state(OvenState* state) {
    // Periodic updates (chart, temp labels)
    switch(uiCtx.current_screen) {
        case UI_SCREEN_DASHBOARD: ui_screen_dashboard_update(state); break;
        case UI_SCREEN_MANUAL:    ui_screen_manual_update(state); break;
        default: break;
    }
}

void ui_tick(void) {
    lv_tick_inc(5); // Called every 5ms
    lv_timer_handler();
}
