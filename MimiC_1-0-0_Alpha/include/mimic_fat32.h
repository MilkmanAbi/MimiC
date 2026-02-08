/**
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║  MimiC FAT32 - Minimal FAT32 Implementation for SD Cards                  ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  No Arduino bloat - raw SPI + FAT32 from scratch                          ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#ifndef MIMIC_FAT32_H
#define MIMIC_FAT32_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ============================================================================
// SD CARD CONFIGURATION
// ============================================================================

#ifndef MIMIC_SD_CS
  #define MIMIC_SD_CS       5
#endif
#ifndef MIMIC_SD_MOSI
  #define MIMIC_SD_MOSI     19
#endif
#ifndef MIMIC_SD_MISO
  #define MIMIC_SD_MISO     16
#endif
#ifndef MIMIC_SD_SCK
  #define MIMIC_SD_SCK      18
#endif

// SD Card commands
#define SD_CMD0             0
#define SD_CMD1             1
#define SD_CMD8             8
#define SD_CMD9             9
#define SD_CMD10            10
#define SD_CMD12            12
#define SD_CMD16            16
#define SD_CMD17            17
#define SD_CMD18            18
#define SD_CMD24            24
#define SD_CMD25            25
#define SD_CMD55            55
#define SD_CMD58            58
#define SD_ACMD41           41

// SD Card types
#define SD_TYPE_UNKNOWN     0
#define SD_TYPE_MMC         1
#define SD_TYPE_SD1         2
#define SD_TYPE_SD2         3
#define SD_TYPE_SDHC        4

#define SD_SECTOR_SIZE      512

// ============================================================================
// FAT32 STRUCTURES
// ============================================================================

typedef struct __attribute__((packed)) {
    uint8_t     jump_boot[3];
    char        oem_name[8];
    uint16_t    bytes_per_sector;
    uint8_t     sectors_per_cluster;
    uint16_t    reserved_sectors;
    uint8_t     num_fats;
    uint16_t    root_entry_count;
    uint16_t    total_sectors_16;
    uint8_t     media_type;
    uint16_t    fat_size_16;
    uint16_t    sectors_per_track;
    uint16_t    num_heads;
    uint32_t    hidden_sectors;
    uint32_t    total_sectors_32;
    
    uint32_t    fat_size_32;
    uint16_t    ext_flags;
    uint16_t    fs_version;
    uint32_t    root_cluster;
    uint16_t    fs_info;
    uint16_t    backup_boot_sector;
    uint8_t     reserved[12];
    uint8_t     drive_number;
    uint8_t     reserved1;
    uint8_t     boot_sig;
    uint32_t    volume_id;
    char        volume_label[11];
    char        fs_type[8];
} Fat32BPB;

typedef struct __attribute__((packed)) {
    char        name[8];
    char        ext[3];
    uint8_t     attr;
    uint8_t     nt_res;
    uint8_t     crt_time_tenth;
    uint16_t    crt_time;
    uint16_t    crt_date;
    uint16_t    lst_acc_date;
    uint16_t    fst_clus_hi;
    uint16_t    wrt_time;
    uint16_t    wrt_date;
    uint16_t    fst_clus_lo;
    uint32_t    file_size;
} Fat32DirEntry;

typedef struct __attribute__((packed)) {
    uint8_t     order;
    uint16_t    name1[5];
    uint8_t     attr;
    uint8_t     type;
    uint8_t     checksum;
    uint16_t    name2[6];
    uint16_t    fst_clus_lo;
    uint16_t    name3[2];
} Fat32LfnEntry;

// File attributes
#define FAT_ATTR_READ_ONLY  0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME_ID  0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20
#define FAT_ATTR_LFN        0x0F

// Special cluster values
#define FAT32_EOC           0x0FFFFFF8
#define FAT32_FREE          0x00000000
#define FAT32_BAD           0x0FFFFFF7

// ============================================================================
// VOLUME STATE
// ============================================================================

typedef struct {
    uint8_t     card_type;
    bool        initialized;
    
    uint32_t    partition_start;    // LBA of partition start (0 for superfloppy)
    uint32_t    sectors_per_cluster;
    uint32_t    bytes_per_cluster;
    uint32_t    fat_start_sector;
    uint32_t    fat_sectors;
    uint32_t    root_cluster;
    uint32_t    data_start_sector;
    uint32_t    total_clusters;
    
    uint32_t    cached_sector;
    uint8_t     sector_buf[SD_SECTOR_SIZE];
    bool        cache_dirty;
} MimicVolume;

// ============================================================================
// FILE HANDLES
// ============================================================================

#define MIMIC_MAX_FILES     8
#define MIMIC_MAX_PATH      256

typedef struct {
    bool        open;
    bool        is_dir;
    uint8_t     mode;
    uint32_t    dir_cluster;
    uint32_t    dir_entry_idx;
    uint32_t    first_cluster;
    uint32_t    current_cluster;
    uint32_t    cluster_offset;
    uint32_t    file_size;
    uint32_t    position;
    char        path[MIMIC_MAX_PATH];
} MimicFile;

// File open modes
#define MIMIC_FILE_READ     0x01
#define MIMIC_FILE_WRITE    0x02
#define MIMIC_FILE_APPEND   0x04
#define MIMIC_FILE_CREATE   0x08
#define MIMIC_FILE_TRUNC    0x10

// Seek modes
#define MIMIC_SEEK_SET      0
#define MIMIC_SEEK_CUR      1
#define MIMIC_SEEK_END      2

// ============================================================================
// SD CARD API
// ============================================================================

int mimic_sd_init(void);
bool mimic_sd_present(void);
int mimic_sd_read_sector(uint32_t sector, uint8_t* buf);
int mimic_sd_write_sector(uint32_t sector, const uint8_t* buf);
int mimic_sd_read_sectors(uint32_t sector, uint8_t* buf, uint32_t count);
int mimic_sd_write_sectors(uint32_t sector, const uint8_t* buf, uint32_t count);
uint8_t mimic_sd_get_type(void);
uint64_t mimic_sd_get_size(void);

// ============================================================================
// FAT32 API
// ============================================================================

int mimic_fat32_mount(void);
void mimic_fat32_unmount(void);
bool mimic_fat32_mounted(void);

int mimic_fopen(const char* path, uint8_t mode);
int mimic_fclose(int fd);
int mimic_fread(int fd, void* buf, size_t size);
int mimic_fwrite(int fd, const void* buf, size_t size);
int mimic_fseek(int fd, int32_t offset, int whence);
int32_t mimic_ftell(int fd);
int mimic_fflush(int fd);
int32_t mimic_fsize(int fd);
bool mimic_feof(int fd);

int mimic_mkdir(const char* path);
int mimic_rmdir(const char* path);
int mimic_chdir(const char* path);
int mimic_getcwd(char* buf, size_t size);

int mimic_unlink(const char* path);
int mimic_rename(const char* old_path, const char* new_path);
int mimic_stat(const char* path, uint32_t* size, uint8_t* attr);
bool mimic_exists(const char* path);
bool mimic_is_dir(const char* path);

typedef struct {
    char        name[256];
    uint32_t    size;
    uint8_t     attr;
    bool        is_dir;
} MimicDirEntry;

int mimic_opendir(const char* path);
int mimic_readdir(int dir_handle, MimicDirEntry* entry);
int mimic_closedir(int dir_handle);

int mimic_read_file(const char* path, void* buf, size_t max_size);
int mimic_write_file(const char* path, const void* buf, size_t size);
int mimic_append_file(const char* path, const void* buf, size_t size);

typedef struct {
    uint64_t    total_bytes;
    uint64_t    free_bytes;
    uint64_t    used_bytes;
    uint32_t    total_clusters;
    uint32_t    free_clusters;
    uint32_t    cluster_size;
    uint32_t    sector_size;
} MimicFSInfo;

int mimic_fs_info(MimicFSInfo* info);

// ============================================================================
// STREAMING I/O FOR COMPILER
// ============================================================================

typedef struct {
    int         fd;
    uint8_t*    buffer;
    uint32_t    buf_size;
    uint32_t    buf_pos;
    uint32_t    buf_len;
    bool        eof;
    bool        writing;  // Track mode for proper flush
} MimicStream;

int mimic_stream_open(MimicStream* stream, const char* path, uint8_t mode, 
                      uint8_t* buffer, uint32_t buf_size);
int mimic_stream_close(MimicStream* stream);
int mimic_stream_getc(MimicStream* stream);
int mimic_stream_ungetc(MimicStream* stream, int c);
int mimic_stream_putc(MimicStream* stream, int c);
int mimic_stream_puts(MimicStream* stream, const char* s);
int mimic_stream_read(MimicStream* stream, void* buf, size_t size);
int mimic_stream_write(MimicStream* stream, const void* buf, size_t size);
int mimic_stream_flush(MimicStream* stream);
bool mimic_stream_eof(MimicStream* stream);

#endif // MIMIC_FAT32_H
