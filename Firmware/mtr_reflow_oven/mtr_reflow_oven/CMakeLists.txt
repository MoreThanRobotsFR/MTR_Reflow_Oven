# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==

set(PICO_BOARD pico CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()

# ====================================================================================

include(pico_sdk_import.cmake)

project(main C CXX ASM)

set(PICO_CXX_ENABLE_EXCEPTIONS 1)

pico_sdk_init()

# === FreeRTOS Kernel (LOCAL) ===
set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib/FreeRTOS-Kernel)
if(EXISTS "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake")
    include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)
else()
    message(FATAL_ERROR "FreeRTOS Import not found")
endif()

set(FREERTOS_INC
    ${FREERTOS_KERNEL_PATH}/include
    ${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/include
)
set(FREERTOS_CFG ${CMAKE_CURRENT_SOURCE_DIR})

# === Executable ===
add_executable(main)

target_sources(main PRIVATE
    mtr_reflow_oven.cpp
    lib/cJSON/cJSON.c
    ui/ui_manager.cpp
    ui/ui_styles.cpp
    ui/ui_screen_menu.cpp
    ui/ui_screen_dashboard.cpp
    ui/ui_screen_manual.cpp
    ui/ui_screen_profile.cpp
    ui/ui_screen_settings.cpp
)

pico_set_program_name(main "main")
pico_set_program_version(main "0.1")

pico_enable_stdio_uart(main 0)
pico_enable_stdio_usb(main 1)

# Add subdirectories for libraries
add_subdirectory(lib/no-OS-FatFS-SD-SPI-RPi-Pico)
add_subdirectory(lib/cJSON)
# HAGL REMOVED

# Manual LVGL Build to ensure Config Visibility
file(GLOB_RECURSE LVGL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/lib/lvgl/src/*.c")
add_library(lvgl STATIC ${LVGL_SOURCES})
target_include_directories(lvgl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib/lvgl)
target_include_directories(lvgl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}) # For lv_conf.h
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)

# MIPI Configuration for ST7796 (Used by LVGL driver in code now)
# We can keep defines if we use them in cpp, or remove them and hardcode in cpp.
# keeping basic defines for driver usage in cpp
target_compile_definitions(main PRIVATE
    MIPI_DISPLAY_PIN_DC=25
    MIPI_DISPLAY_PIN_CS=21
    MIPI_DISPLAY_PIN_CLK=18
    MIPI_DISPLAY_PIN_MOSI=19
    MIPI_DISPLAY_PIN_RST=24
    MIPI_DISPLAY_PIN_BL=23
    
    MIPI_DISPLAY_SPI_PORT=spi0
    MIPI_DISPLAY_SPI_CLOCK_SPEED_HZ=40000000 # Boost to 40MHz for LVGL speed?
)

# Includes
target_include_directories(main PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/ui
        ${FREERTOS_INC}
        ${FREERTOS_CFG}
        ${CMAKE_CURRENT_LIST_DIR}/lib/cJSON
        lib/no-OS-FatFS-SD-SPI-RPi-Pico
)

# Links
target_link_libraries(main
        pico_stdlib
        pico_multicore
        hardware_i2c
        hardware_dma
        hardware_spi
        hardware_gpio
        hardware_pwm
        hardware_adc
        hardware_interp
        hardware_pio
        FreeRTOS-Kernel
        FreeRTOS-Kernel-Heap4
        FatFs_SPI
        cJSON
        cJSON
        lvgl

)

pico_add_extra_outputs(main)
