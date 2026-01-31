#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// --- Inputs (Control Panel) ---
#define GPIO_BTN1      0  // Input (Confirm/Start)
#define GPIO_BTN2      1  // Input (Stop/Back)

#define GPIO_SD_CD     3  // Input (SD Card Detect)

// --- I2C Bus (Sensors) ---
#define GPIO_I2C_SDA   4
#define GPIO_I2C_SCL   5
#define I2C_PORT       i2c0
#define I2C_ADDR_MCP9600_T1 0x60 // Default address for Sensor 1
#define I2C_ADDR_MCP9600_T2 0x67 // Default address for Sensor 2 (if present)

// --- Rotary Encoder ---
#define GPIO_ROT_DT    6
#define GPIO_ROT_CLK   7
#define GPIO_ROT_BTN   8

// --- Status LEDs ---
#define GPIO_WS_LED    9  // WS2812B Data

// --- MCP9600 Alerts (Sensor 1) ---
#define GPIO_T1_ALT4   10
#define GPIO_T1_ALT3   11
#define GPIO_T1_ALT2   12
#define GPIO_T1_ALT1   13

// --- MCP9600 Alerts (Sensor 2) ---
#define GPIO_T2_ALT4   14
#define GPIO_T2_ALT3   15
#define GPIO_T2_ALT2   16
#define GPIO_T2_ALT1   17

// --- SPI Bus (Screen + SD) ---
#define GPIO_SPI_SCK   18
#define GPIO_SPI_MOSI  19
#define GPIO_SPI_MISO  20
#define SPI_PORT       spi0

// --- Chip Selects & Control ---
#define GPIO_SCR_CS    21
#define GPIO_SD_CS     22
#define GPIO_SCR_BL    23 // PWM
#define GPIO_SCR_RST   24
#define GPIO_SCR_DC    25

// --- Power Outputs (SSRs) ---
#define GPIO_HEAT1     26 // PWM (SSR 1)
#define GPIO_HEAT2     27 // PWM (SSR 2)

// --- Audio ---
#define GPIO_BUZZER    28 // PWM

// --- 1-Wire ---
#define GPIO_ONE_WIRE  29


#endif // BOARD_CONFIG_H
