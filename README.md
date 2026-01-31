# MTR Reflow Oven

**MTR Reflow Oven** is a comprehensive hardware and firmware project designed to control a reflow oven for PCB assembly. This repository contains the custom PCB design, the firmware for the RP2040 microcontroller, and a companion PC configurator tool.

## Repository Structure

*   **`Firmware/`**
    *   **`mtr_reflow_oven/`**: The core C++ firmware for the RP2040 microcontroller. It handles temperature PID control, user interface (LVGL), and profile management.
    *   **`Configurator/`**: A Python-based desktop application for creating and managing reflow profiles and configuring device settings.
*   **`Hardware/`**
    *   Contains the KiCad project files (`.kicad_pcb`, `.kicad_sch`) for the custom controller board.

## Getting Started

### Hardware
The `Hardware` directory contains the complete KiCad project. You can open `Hardware/MTR_Reflow_Oven.kicad_pro` with [KiCad 8.0+](https://www.kicad.org/) to view or modify the schematics and PCB layout.

### Firmware (RP2040)
The firmware is built using the Raspberry Pi Pico SDK.

**Prerequisites:**
*   CMake
*   Arm GNU Toolchain (`arm-none-eabi-gcc`)
*   Pico SDK

**Build Instructions:**
1.  Navigate to the firmware directory:
    ```bash
    cd Firmware/mtr_reflow_oven
    ```
2.  Create a build directory:
    ```bash
    mkdir build
    cd build
    ```
3.  Configure and build:
    ```bash
    cmake ..
    make
    ```
4.  Flash the resulting `.uf2` file to your RP2040 device.

### Configurator (PC Software)
The configurator allows you to easily edit reflow profiles.

**Prerequisites:**
*   Python 3.x
*   Required packages (see `requirements.txt` if available, or manually install common dependencies like `pyserial`, `tkinter`/`customtkinter`).

**Running:**
```bash
cd Firmware/Configurator
python reflow_configurator.py
```

## License
[License Information Here]
