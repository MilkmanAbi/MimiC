/**
 * FatFS Glue Code for RP2040
 * 
 * Bridges FatFS to RP2040 SPI hardware for SD card access
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

// FatFS will be added as a library later
// This file will contain:
// - SPI initialization for SD card
// - Disk I/O functions required by FatFS
// - SD card low-level commands (CMD0, CMD1, ACMD41, etc.)

/**
 * SD card pin configuration
 */
#define SD_SPI_PORT   spi0
#define SD_PIN_MISO   4
#define SD_PIN_CS     5
#define SD_PIN_SCK    2
#define SD_PIN_MOSI   3

/**
 * Initialize SPI for SD card
 */
void fatfs_spi_init(void) {
    // Initialize SPI
    spi_init(SD_SPI_PORT, 400 * 1000); // Start at 400kHz for initialization
    
    // Set SPI format
    spi_set_format(SD_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    
    // Initialize GPIO pins
    gpio_set_function(SD_PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(SD_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SD_PIN_MOSI, GPIO_FUNC_SPI);
    
    // CS is manual
    gpio_init(SD_PIN_CS);
    gpio_set_dir(SD_PIN_CS, GPIO_OUT);
    gpio_put(SD_PIN_CS, 1); // Deselect
}

// TODO: Implement FatFS disk I/O functions:
// - disk_initialize()
// - disk_status()
// - disk_read()
// - disk_write()
// - disk_ioctl()
// - get_fattime()
