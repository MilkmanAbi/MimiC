# ╔════════════════════════════════════════════════════════════════════════════╗
# ║  MimiC - Self-Hosted C Compiler for RP2040/RP2350                          ║
# ║  CMake Build Configuration                                                  ║
# ╚════════════════════════════════════════════════════════════════════════════╝

cmake_minimum_required(VERSION 3.13)

# Include Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(mimic C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

# ============================================================================
# TARGET CONFIGURATION
# ============================================================================

option(MIMIC_TARGET_RP2350 "Build for RP2350 (default: RP2040)" OFF)

if(MIMIC_TARGET_RP2350)
    add_compile_definitions(MIMIC_TARGET_RP2350=1)
    set(PICO_BOARD pico2)
else()
    add_compile_definitions(MIMIC_TARGET_RP2350=0)
    set(PICO_BOARD pico)
endif()

# ============================================================================
# SOURCE FILES
# ============================================================================

set(MIMIC_SOURCES
    src/main.c
    src/kernel/mimic_kernel.c
    src/fs/mimic_fat32.c
    src/compiler/mimic_compiler.c
)

set(MIMIC_HEADERS
    include/mimic.h
    include/mimic_fat32.h
)

# ============================================================================
# MAIN EXECUTABLE
# ============================================================================

add_executable(mimic ${MIMIC_SOURCES})

target_include_directories(mimic PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(mimic
    pico_stdlib
    pico_multicore
    pico_sync
    hardware_spi
    hardware_gpio
    hardware_adc
    hardware_pwm
    hardware_timer
    hardware_watchdog
)

# Enable USB stdio for debugging
pico_enable_stdio_usb(mimic 1)
pico_enable_stdio_uart(mimic 0)

# Create UF2 file for flashing
pico_add_extra_outputs(mimic)

# ============================================================================
# COMPILER FLAGS
# ============================================================================

target_compile_options(mimic PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-unused-function
    -Os
    -ffunction-sections
    -fdata-sections
)

target_link_options(mimic PRIVATE
    -Wl,--gc-sections
)

# ============================================================================
# CUSTOM TARGETS
# ============================================================================

# Flash target (requires picotool)
add_custom_target(flash
    COMMAND picotool load -f ${CMAKE_BINARY_DIR}/mimic.uf2
    DEPENDS mimic
    COMMENT "Flashing MimiC to device..."
)

# Size report
add_custom_target(size
    COMMAND ${CMAKE_SIZE} ${CMAKE_BINARY_DIR}/mimic.elf
    DEPENDS mimic
    COMMENT "Code size report:"
)

# ============================================================================
# INSTALL
# ============================================================================

install(TARGETS mimic DESTINATION bin)
install(FILES ${MIMIC_HEADERS} DESTINATION include/mimic)
