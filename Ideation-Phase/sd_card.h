/**
 * SD Card Interface
 * 
 * Simple wrapper around FatFS for SD card access
 */

#ifndef SD_CARD_H
#define SD_CARD_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize SD card and mount filesystem
 * 
 * @return 0 on success, -1 on failure
 */
int sd_card_init(void);

/**
 * Read entire file from SD card into memory
 * 
 * @param path File path (e.g. "/examples/blink.c")
 * @param buffer_out Output buffer (allocated by this function)
 * @param size_out Output file size
 * @return 0 on success, -1 on failure
 * 
 * Note: Caller must free buffer with vPortFree()
 */
int sd_read_file(const char *path, char **buffer_out, size_t *size_out);

/**
 * Write file to SD card
 * 
 * @param path File path
 * @param buffer Data to write
 * @param size Size of data
 * @return 0 on success, -1 on failure
 */
int sd_write_file(const char *path, const char *buffer, size_t size);

/**
 * Check if file exists
 * 
 * @param path File path
 * @return 1 if exists, 0 if not
 */
int sd_file_exists(const char *path);

/**
 * List files in directory
 * 
 * @param path Directory path
 * @param callback Callback for each file (return 1 to continue, 0 to stop)
 */
typedef int (*sd_list_callback_t)(const char *filename, int is_dir, size_t size);
int sd_list_directory(const char *path, sd_list_callback_t callback);

/**
 * Get SD card info
 */
typedef struct {
    uint64_t total_bytes;
    uint64_t free_bytes;
    int mounted;
} SDCardInfo;

void sd_get_info(SDCardInfo *info);

#ifdef __cplusplus
}
#endif

#endif /* SD_CARD_H */
