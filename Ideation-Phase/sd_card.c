/**
 * SD Card Implementation
 * 
 * Uses FatFS for filesystem access via SPI
 */

#include "sd_card.h"
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"

/* FatFS includes - will need to add FatFS library */
// #include "ff.h"

static int sd_mounted = 0;

/**
 * Initialize SD card
 */
int sd_card_init(void) {
    printf("[SD] Initializing SD card...\n");
    
    // TODO: Initialize SPI for SD card
    // Typical RP2040 SD card wiring:
    // - CS:   GPIO 5
    // - SCK:  GPIO 2
    // - MOSI: GPIO 3
    // - MISO: GPIO 4
    
    // TODO: Mount filesystem with FatFS
    // FATFS fs;
    // FRESULT res = f_mount(&fs, "", 1);
    // if (res != FR_OK) {
    //     printf("[SD] Mount failed: %d\n", res);
    //     return -1;
    // }
    
    sd_mounted = 0; // Set to 1 when actually working
    
    printf("[SD] SD card not implemented yet\n");
    return -1;
}

/**
 * Read file from SD card
 */
int sd_read_file(const char *path, char **buffer_out, size_t *size_out) {
    if (!sd_mounted) {
        printf("[SD] SD card not mounted\n");
        return -1;
    }
    
    printf("[SD] Reading file: %s\n", path);
    
    // TODO: Open file with FatFS
    // FIL file;
    // FRESULT res = f_open(&file, path, FA_READ);
    // if (res != FR_OK) {
    //     printf("[SD] Failed to open file: %d\n", res);
    //     return -1;
    // }
    
    // Get file size
    // FSIZE_t file_size = f_size(&file);
    
    // Allocate buffer
    // char *buffer = pvPortMalloc(file_size + 1);
    // if (!buffer) {
    //     f_close(&file);
    //     return -1;
    // }
    
    // Read file
    // UINT bytes_read;
    // res = f_read(&file, buffer, file_size, &bytes_read);
    // f_close(&file);
    
    // if (res != FR_OK || bytes_read != file_size) {
    //     vPortFree(buffer);
    //     return -1;
    // }
    
    // buffer[file_size] = '\0';
    // *buffer_out = buffer;
    // *size_out = file_size;
    
    return -1;
}

/**
 * Write file to SD card
 */
int sd_write_file(const char *path, const char *buffer, size_t size) {
    if (!sd_mounted) {
        printf("[SD] SD card not mounted\n");
        return -1;
    }
    
    printf("[SD] Writing file: %s (%zu bytes)\n", path, size);
    
    // TODO: Implement with FatFS
    // FIL file;
    // FRESULT res = f_open(&file, path, FA_WRITE | FA_CREATE_ALWAYS);
    // if (res != FR_OK) {
    //     return -1;
    // }
    
    // UINT bytes_written;
    // res = f_write(&file, buffer, size, &bytes_written);
    // f_close(&file);
    
    // return (res == FR_OK && bytes_written == size) ? 0 : -1;
    
    return -1;
}

/**
 * Check if file exists
 */
int sd_file_exists(const char *path) {
    if (!sd_mounted) {
        return 0;
    }
    
    // TODO: Check with FatFS
    // FILINFO fno;
    // FRESULT res = f_stat(path, &fno);
    // return (res == FR_OK) ? 1 : 0;
    
    return 0;
}

/**
 * List directory
 */
int sd_list_directory(const char *path, sd_list_callback_t callback) {
    if (!sd_mounted || !callback) {
        return -1;
    }
    
    printf("[SD] Listing directory: %s\n", path);
    
    // TODO: List with FatFS
    // DIR dir;
    // FILINFO fno;
    
    // FRESULT res = f_opendir(&dir, path);
    // if (res != FR_OK) {
    //     return -1;
    // }
    
    // while (1) {
    //     res = f_readdir(&dir, &fno);
    //     if (res != FR_OK || fno.fname[0] == 0) {
    //         break;
    //     }
    //     
    //     int is_dir = (fno.fattrib & AM_DIR) ? 1 : 0;
    //     if (!callback(fno.fname, is_dir, fno.fsize)) {
    //         break;
    //     }
    // }
    
    // f_closedir(&dir);
    
    return -1;
}

/**
 * Get SD card info
 */
void sd_get_info(SDCardInfo *info) {
    if (!info) {
        return;
    }
    
    memset(info, 0, sizeof(SDCardInfo));
    info->mounted = sd_mounted;
    
    if (!sd_mounted) {
        return;
    }
    
    // TODO: Get card size with FatFS
    // FATFS *fs;
    // DWORD fre_clust;
    
    // f_getfree("", &fre_clust, &fs);
    
    // info->total_bytes = (uint64_t)(fs->n_fatent - 2) * fs->csize * 512;
    // info->free_bytes = (uint64_t)fre_clust * fs->csize * 512;
}
