cmake_minimum_required(VERSION 3.13)

# Pull in Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(mimic C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

# TCC configuration
set(TCC_VERSION "0.9.27")
set(TCC_TARGET "arm-eabi")

# FreeRTOS configuration
set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_LIST_DIR}/lib/FreeRTOS-Kernel)
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

# Main executable
add_executable(mimic
    src/main.c
    src/compiler/tcc_port.c
    src/compiler/symbol_table.c
    src/filesystem/sd_card.c
    src/filesystem/fatfs_glue.c
    src/runtime/executor.c
    src/shell/uart_shell.c
)

# Include directories
target_include_directories(mimic PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/src/compiler
    ${CMAKE_CURRENT_LIST_DIR}/src/filesystem
    ${CMAKE_CURRENT_LIST_DIR}/src/runtime
    ${CMAKE_CURRENT_LIST_DIR}/lib/tcc
    ${CMAKE_CURRENT_LIST_DIR}/lib/fatfs/source
)

# Link libraries
target_link_libraries(mimic
    pico_stdlib
    pico_multicore
    hardware_spi
    hardware_gpio
    hardware_dma
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4
)

# Enable USB output, disable UART output (can change as needed)
pico_enable_stdio_usb(mimic 1)
pico_enable_stdio_uart(mimic 0)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(mimic)

# Build TCC library
add_subdirectory(lib/tcc)
target_link_libraries(mimic tcc_embedded)
