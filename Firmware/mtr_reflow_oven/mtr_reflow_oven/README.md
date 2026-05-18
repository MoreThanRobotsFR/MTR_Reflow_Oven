# MTR Reflow Oven Controller

Firmware for the MoreThanRobotsFR (MTR) Reflow Oven, based on the Raspberry Pi Pico (RP2040).

## Overview

This project implements a complete reflow oven controller utilizing FreeRTOS for task management and LVGL for the graphical user interface. It is designed to run on the RP2040 microcontroller and controls up to two solid-state relays (SSRs) to manage heating elements using a custom PID control loop.

## Key Features

- **Microcontroller**: RP2040 (Raspberry Pi Pico)
- **Real-Time OS**: FreeRTOS
- **User Interface**: 
  - SPI Display (e.g., ST7796) driven by the LVGL graphics library
  - Navigation via Rotary Encoder and tactile buttons
  - Multi-screen UI (Dashboard, Profile Selection, Settings, Manual Control)
- **Temperature Sensing**: Support for up to two MCP9600 Thermocouple Amplifiers via I2C
- **Heater Control**: 
  - Dual PWM outputs for SSR (Solid State Relay) control
  - Configurable PID loop for precise temperature tracking
- **Storage**: SD Card support (FatFS over SPI) for loading JSON-based reflow profiles and saving system configurations
- **Feedback**: 
  - WS2812B RGB LED status indicator (using PIO)
  - Passive buzzer for audible alerts

## Hardware Configuration

The pinout and hardware settings are defined in `board_config.h`. 

Key pin assignments:
- **Display (SPI0)**: SCK (18), MOSI (19), MISO (20), CS (21), DC (25), RST (24), Backlight (23)
- **SD Card (SPI0)**: CS (22)
- **Sensors (I2C0)**: SDA (4), SCL (5)
- **Heaters**: SSR1 (26), SSR2 (27)
- **Inputs**: 
  - Rotary Encoder: DT (6), CLK (7), SW (8)
  - Buttons: BTN1 (0), BTN2 (1)
- **Outputs**: WS2812B LED (9), Buzzer (28)

## Build Instructions

This project uses CMake and requires the Raspberry Pi Pico SDK.

1. Ensure you have the [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) installed and the `PICO_SDK_PATH` environment variable set.
2. Initialize and build the project:
   ```bash
   mkdir build
   cd build
   cmake ..
   make # or ninja, depending on your environment
   ```
3. Flash the resulting `.uf2` file to your Pico by holding the BOOTSEL button while plugging it in, then dragging the file to the `RPI-RP2` drive.

## Configuration & Profiles

- **System Config**: Settings such as PID tuning parameters, display orientation, and hardware flags are stored on the SD Card at `/config/system.json`.
- **Reflow Profiles**: Custom temperature profiles (e.g., SAC305) can be loaded from the SD Card in JSON format. They define specific heating segments (Preheat, Soak, Ramp, Reflow) with target temperatures and durations.
