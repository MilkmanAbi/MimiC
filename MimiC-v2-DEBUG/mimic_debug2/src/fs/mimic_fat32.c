/**
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║  MimiC FAT32 - Minimal FAT32 Implementation                               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  Raw SPI + FAT32 - no bloat                                               ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

#include <string.h>
#include <stdio.h>

#include "mimic.h"
#include "mimic_fat32.h"

// ============================================================================
// SPI CONFIGURATION
// ============================================================================

#define SD_SPI              spi0
#define SD_BAUDRATE_SLOW    400000      // 400kHz for init
#define SD_BAUDRATE_FAST    4000000     // 4MHz for operation (very conservative)

// ============================================================================
// GLOBAL STATE
// ============================================================================

static MimicVolume vol;
static MimicFile files[MIMIC_MAX_FILES];
static char current_dir[MIMIC_MAX_PATH] = "/";

// ============================================================================
// LOW-LEVEL SPI
// ============================================================================

static inline void sd_cs_low(void) {
    asm volatile("nop \n nop \n nop");
    gpio_put(MIMIC_SD_CS, 0);
    asm volatile("nop \n nop \n nop");
}

static inline void sd_cs_high(void) {
    asm volatile("nop \n nop \n nop");
    gpio_put(MIMIC_SD_CS, 1);
    asm volatile("nop \n nop \n nop");
}

static uint8_t sd_spi_xfer(uint8_t tx) {
    uint8_t rx;
    spi_write_read_blocking(SD_SPI, &tx, &rx, 1);
    return rx;
}

static void sd_spi_write(const uint8_t* data, size_t len) {
    spi_write_blocking(SD_SPI, data, len);
}

static void sd_spi_read(uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        data[i] = sd_spi_xfer(0xFF);
    }
}

static void sd_dummy_clocks(int count) {
    for (int i = 0; i < count; i++) {
        sd_spi_xfer(0xFF);
    }
}

static bool sd_wait_ready(uint32_t timeout_ms) {
    uint32_t start = to_ms_since_boot(get_absolute_time());
    do {
        if (sd_spi_xfer(0xFF) == 0xFF) return true;
    } while ((to_ms_since_boot(get_absolute_time()) - start) < timeout_ms);
    return false;
}

// ============================================================================
// SD COMMANDS
// ============================================================================

static uint8_t sd_cmd(uint8_t cmd, uint32_t arg) {
    uint8_t buf[6];
    buf[0] = 0x40 | cmd;
    buf[1] = (arg >> 24) & 0xFF;
    buf[2] = (arg >> 16) & 0xFF;
    buf[3] = (arg >> 8) & 0xFF;
    buf[4] = arg & 0xFF;
    
    // CRC (only matters for CMD0 and CMD8 in SPI mode)
    if (cmd == SD_CMD0) buf[5] = 0x95;
    else if (cmd == SD_CMD8) buf[5] = 0x87;
    else buf[5] = 0x01;
    
    // Send command
    sd_spi_write(buf, 6);
    
    // Skip stuff byte for stop transmission
    if (cmd == SD_CMD12) sd_spi_xfer(0xFF);
    
    // Wait for response (up to 8 bytes)
    uint8_t resp;
    for (int i = 0; i < 8; i++) {
        resp = sd_spi_xfer(0xFF);
        if (!(resp & 0x80)) break;
    }
    return resp;
}

static uint8_t sd_acmd(uint8_t cmd, uint32_t arg) {
    sd_cmd(SD_CMD55, 0);
    sd_spi_xfer(0xFF);
    return sd_cmd(cmd, arg);
}

// ============================================================================
// SD CARD INIT - BULLETPROOF VERSION
// ============================================================================

int mimic_sd_init(void) {
    printf("[SD] Init start\n");
    
    // 1. Configure CS pin FIRST, keep HIGH
    gpio_init(MIMIC_SD_CS);
    gpio_set_dir(MIMIC_SD_CS, GPIO_OUT);
    gpio_put(MIMIC_SD_CS, 1);
    
    // 2. Initialize SPI peripheral at slow speed
    spi_init(SD_SPI, SD_BAUDRATE_SLOW);
    spi_set_format(SD_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    
    // 3. Configure SPI pins
    gpio_set_function(MIMIC_SD_SCK, GPIO_FUNC_SPI);
    gpio_set_function(MIMIC_SD_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(MIMIC_SD_MISO, GPIO_FUNC_SPI);
    
    printf("[SD] SPI0 on CS=%d MOSI=%d MISO=%d SCK=%d\n",
           MIMIC_SD_CS, MIMIC_SD_MOSI, MIMIC_SD_MISO, MIMIC_SD_SCK);
    
    vol.card_type = SD_TYPE_UNKNOWN;
    vol.initialized = false;
    
    // 4. Power-on delay
    sleep_ms(100);
    
    // 5. Send 80+ clocks with CS HIGH to wake up card
    printf("[SD] Sending wake-up clocks\n");
    sd_cs_high();
    for (int i = 0; i < 10; i++) {
        sd_spi_xfer(0xFF);
    }
    
    // 6. CMD0 - Reset card to SPI mode
    printf("[SD] CMD0 (go idle)\n");
    uint8_t r1 = 0xFF;
    for (int retry = 0; retry < 20; retry++) {
        sd_cs_low();
        sd_spi_xfer(0xFF);
        r1 = sd_cmd(SD_CMD0, 0);
        sd_cs_high();
        sd_spi_xfer(0xFF);
        
        printf("[SD]   try %d: r1=0x%02X\n", retry, r1);
        if (r1 == 0x01) break;
        sleep_ms(10);
    }
    
    if (r1 != 0x01) {
        printf("[SD] FAIL: No response to CMD0\n");
        return MIMIC_ERR_IO;
    }
    
    // 7. CMD8 - Check for SDv2/SDHC
    printf("[SD] CMD8 (interface condition)\n");
    sd_cs_low();
    r1 = sd_cmd(SD_CMD8, 0x000001AA);
    
    bool is_sdv2 = false;
    if (r1 == 0x01) {
        uint8_t ocr[4];
        sd_spi_read(ocr, 4);
        sd_cs_high();
        sd_spi_xfer(0xFF);
        
        printf("[SD]   R7: %02X %02X %02X %02X\n", ocr[0], ocr[1], ocr[2], ocr[3]);
        
        if (ocr[2] == 0x01 && ocr[3] == 0xAA) {
            is_sdv2 = true;
            printf("[SD]   SDv2 detected\n");
        } else {
            printf("[SD] FAIL: Bad CMD8 response pattern\n");
            return MIMIC_ERR_IO;
        }
    } else {
        sd_cs_high();
        sd_spi_xfer(0xFF);
        printf("[SD]   SDv1/MMC (CMD8 rejected)\n");
    }
    
    // 8. Initialize card with ACMD41 or CMD1
    printf("[SD] Initializing card...\n");
    uint32_t start = to_ms_since_boot(get_absolute_time());
    
    while (1) {
        if ((to_ms_since_boot(get_absolute_time()) - start) > 2000) {
            printf("[SD] FAIL: Init timeout\n");
            return MIMIC_ERR_IO;
        }
        
        sd_cs_low();
        if (is_sdv2) {
            r1 = sd_acmd(SD_ACMD41, 0x40000000);  // HCS bit for SDHC
        } else {
            r1 = sd_acmd(SD_ACMD41, 0);
            if (r1 > 1) {
                // Not SD card, try MMC with CMD1
                r1 = sd_cmd(SD_CMD1, 0);
                vol.card_type = SD_TYPE_MMC;
            }
        }
        sd_cs_high();
        sd_spi_xfer(0xFF);
        
        if (r1 == 0x00) break;
        sleep_ms(10);
    }
    
    printf("[SD] Card ready (r1=0x%02X)\n", r1);
    
    // 9. Check CCS bit for SDHC
    if (is_sdv2) {
        sd_cs_low();
        r1 = sd_cmd(SD_CMD58, 0);
        if (r1 == 0x00) {
            uint8_t ocr[4];
            sd_spi_read(ocr, 4);
            printf("[SD] OCR: %02X %02X %02X %02X\n", ocr[0], ocr[1], ocr[2], ocr[3]);
            vol.card_type = (ocr[0] & 0x40) ? SD_TYPE_SDHC : SD_TYPE_SD2;
        }
        sd_cs_high();
        sd_spi_xfer(0xFF);
    } else if (vol.card_type != SD_TYPE_MMC) {
        vol.card_type = SD_TYPE_SD1;
    }
    
    // 10. Set block size to 512 (required for non-SDHC)
    if (vol.card_type != SD_TYPE_SDHC) {
        sd_cs_low();
        sd_cmd(SD_CMD16, 512);
        sd_cs_high();
        sd_spi_xfer(0xFF);
    }
    
    // 11. Switch to fast SPI
    spi_set_baudrate(SD_SPI, SD_BAUDRATE_FAST);
    
    // Send dummy clocks after speed change
    sd_cs_high();
    for (int i = 0; i < 10; i++) {
        sd_spi_xfer(0xFF);
    }
    
    const char* types[] = {"?", "MMC", "SD1", "SD2", "SDHC"};
    printf("[SD] SUCCESS: %s card\n", types[vol.card_type]);
    
    vol.initialized = true;
    return MIMIC_OK;
}

bool mimic_sd_present(void) {
    return vol.initialized;
}

uint8_t mimic_sd_get_type(void) {
    return vol.card_type;
}

// ============================================================================
// SECTOR READ/WRITE
// ============================================================================

int mimic_sd_read_sector(uint32_t sector, uint8_t* buf) {
    if (!vol.initialized) return MIMIC_ERR_IO;
    
    uint32_t addr = (vol.card_type == SD_TYPE_SDHC) ? sector : sector * 512;
    
    sd_cs_low();
    
    // Wait for card to be ready
    if (!sd_wait_ready(500)) {
        printf("[SD] Read: card not ready\n");
        sd_cs_high();
        sd_spi_xfer(0xFF);
        return MIMIC_ERR_IO;
    }
    
    uint8_t resp = sd_cmd(SD_CMD17, addr);
    
    if (resp != 0x00) {
        printf("[SD] Read CMD17 failed: 0x%02X (sector %lu)\n", resp, (unsigned long)sector);
        sd_cs_high();
        sd_spi_xfer(0xFF);
        return MIMIC_ERR_IO;
    }
    
    // Wait for data token (0xFE) or error token (0x01-0x1F)
    uint32_t start = to_ms_since_boot(get_absolute_time());
    do {
        resp = sd_spi_xfer(0xFF);
        if (resp != 0xFF) break;
    } while ((to_ms_since_boot(get_absolute_time()) - start) < 500);
    
    if (resp != 0xFE) {
        printf("[SD] Read: no data token, got 0x%02X\n", resp);
        sd_cs_high();
        sd_spi_xfer(0xFF);
        return MIMIC_ERR_IO;
    }
    
    // Read data
    sd_spi_read(buf, 512);
    
    // Discard CRC
    sd_spi_xfer(0xFF);
    sd_spi_xfer(0xFF);
    
    sd_cs_high();
    sd_spi_xfer(0xFF);  // Extra clocks
    
    return MIMIC_OK;
}

int mimic_sd_write_sector(uint32_t sector, const uint8_t* buf) {
    if (!vol.initialized) return MIMIC_ERR_IO;
    
    uint32_t addr = (vol.card_type == SD_TYPE_SDHC) ? sector : sector * 512;
    
    sd_cs_low();
    uint8_t resp = sd_cmd(SD_CMD24, addr);
    
    if (resp != 0x00) {
        sd_cs_high();
        return MIMIC_ERR_IO;
    }
    
    sd_spi_xfer(0xFF);
    sd_spi_xfer(0xFE);  // Data token
    
    sd_spi_write(buf, 512);
    
    // Dummy CRC
    sd_spi_xfer(0xFF);
    sd_spi_xfer(0xFF);
    
    // Check response
    resp = sd_spi_xfer(0xFF);
    if ((resp & 0x1F) != 0x05) {
        sd_cs_high();
        return MIMIC_ERR_IO;
    }
    
    // Wait for write complete
    while (sd_spi_xfer(0xFF) == 0x00);
    
    sd_cs_high();
    return MIMIC_OK;
}

// ============================================================================
// FAT32 INTERNAL HELPERS
// ============================================================================

static int fat32_read_sector(uint32_t sector) {
    if (vol.cached_sector == sector) return MIMIC_OK;
    
    if (vol.cache_dirty) {
        int err = mimic_sd_write_sector(vol.cached_sector, vol.sector_buf);
        if (err != MIMIC_OK) return err;
        vol.cache_dirty = false;
    }
    
    int err = mimic_sd_read_sector(sector, vol.sector_buf);
    if (err == MIMIC_OK) {
        vol.cached_sector = sector;
    }
    return err;
}

static void fat32_cache_dirty(void) {
    vol.cache_dirty = true;
}

static int fat32_flush_cache(void) {
    if (vol.cache_dirty) {
        int err = mimic_sd_write_sector(vol.cached_sector, vol.sector_buf);
        if (err != MIMIC_OK) return err;
        vol.cache_dirty = false;
    }
    return MIMIC_OK;
}

static uint32_t fat32_cluster_to_sector(uint32_t cluster) {
    return vol.data_start_sector + (cluster - 2) * vol.sectors_per_cluster;
}

static uint32_t fat32_get_fat_entry(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t sector = vol.fat_start_sector + (fat_offset / 512);
    uint32_t offset = fat_offset % 512;
    
    if (fat32_read_sector(sector) != MIMIC_OK) return FAT32_EOC;
    
    uint32_t value;
    memcpy(&value, vol.sector_buf + offset, 4);
    return value & 0x0FFFFFFF;
}

static int fat32_set_fat_entry(uint32_t cluster, uint32_t value) {
    uint32_t fat_offset = cluster * 4;
    uint32_t sector = vol.fat_start_sector + (fat_offset / 512);
    uint32_t offset = fat_offset % 512;
    
    if (fat32_read_sector(sector) != MIMIC_OK) return MIMIC_ERR_IO;
    
    uint32_t old;
    memcpy(&old, vol.sector_buf + offset, 4);
    value = (old & 0xF0000000) | (value & 0x0FFFFFFF);
    memcpy(vol.sector_buf + offset, &value, 4);
    
    fat32_cache_dirty();
    return MIMIC_OK;
}

static uint32_t fat32_alloc_cluster(void) {
    for (uint32_t c = 2; c < vol.total_clusters + 2; c++) {
        if (fat32_get_fat_entry(c) == FAT32_FREE) {
            fat32_set_fat_entry(c, FAT32_EOC);
            return c;
        }
    }
    return 0;
}

// Forward declaration
static void fat32_name_to_83(const char* name, char* name83);

// Create a new file in a directory
// Returns directory cluster and entry index for later updates
static int fat32_create_file(uint32_t dir_cluster, const char* name, 
                              Fat32DirEntry* out_entry, 
                              uint32_t* out_dir_cluster, uint32_t* out_dir_entry_idx) {
    char name83[11];
    fat32_name_to_83(name, name83);
    
    // Find a free entry in the directory
    uint32_t cur_cluster = dir_cluster;
    uint32_t entry_idx = 0;
    
    while (cur_cluster != 0 && cur_cluster < FAT32_EOC) {
        for (uint32_t s = 0; s < vol.sectors_per_cluster; s++) {
            uint32_t sector = fat32_cluster_to_sector(cur_cluster) + s;
            if (fat32_read_sector(sector) != MIMIC_OK) return MIMIC_ERR_IO;
            
            Fat32DirEntry* entries = (Fat32DirEntry*)vol.sector_buf;
            
            for (int e = 0; e < 16; e++) {
                // Check for free entry (0x00 = never used, 0xE5 = deleted)
                if (entries[e].name[0] == 0x00 || entries[e].name[0] == (char)0xE5) {
                    // Found a free slot - create the entry
                    memset(&entries[e], 0, sizeof(Fat32DirEntry));
                    memcpy(entries[e].name, name83, 8);
                    memcpy(entries[e].ext, name83 + 8, 3);
                    entries[e].attr = FAT_ATTR_ARCHIVE;
                    entries[e].file_size = 0;
                    entries[e].fst_clus_hi = 0;
                    entries[e].fst_clus_lo = 0;
                    
                    // Set timestamps (simple: all zeros for now)
                    entries[e].crt_time = 0;
                    entries[e].crt_date = 0;
                    entries[e].wrt_time = 0;
                    entries[e].wrt_date = 0;
                    
                    fat32_cache_dirty();
                    fat32_flush_cache();
                    
                    if (out_entry) *out_entry = entries[e];
                    if (out_dir_cluster) *out_dir_cluster = cur_cluster;
                    if (out_dir_entry_idx) *out_dir_entry_idx = entry_idx + e;
                    return MIMIC_OK;
                }
            }
            entry_idx += 16;
        }
        cur_cluster = fat32_get_fat_entry(cur_cluster);
    }
    
    // Directory is full - would need to allocate new cluster
    // For now just return error
    return MIMIC_ERR_NOMEM;
}

// ============================================================================
// FAT32 MOUNT
// ============================================================================

int mimic_fat32_mount(void) {
    int err = mimic_sd_init();
    if (err != MIMIC_OK) return err;
    
    vol.cached_sector = 0xFFFFFFFF;
    vol.cache_dirty = false;
    
    // Read sector 0 (could be MBR or boot sector)
    err = fat32_read_sector(0);
    if (err != MIMIC_OK) {
        printf("[FS] Failed to read sector 0\n");
        return err;
    }
    
    uint32_t partition_start = 0;
    
    // Check for MBR signature (0x55AA at end)
    if (vol.sector_buf[510] == 0x55 && vol.sector_buf[511] == 0xAA) {
        // Check if this is a boot sector or MBR
        // FAT32 boot sector has "FAT32" at offset 82
        // MBR has partition table at offset 446
        
        if (vol.sector_buf[0] != 0xEB && vol.sector_buf[0] != 0xE9) {
            // Likely MBR, not a boot sector (boot sectors start with JMP)
            // Parse first partition entry at offset 446
            uint8_t* part = &vol.sector_buf[446];
            
            // Partition type at offset 4
            uint8_t part_type = part[4];
            printf("[FS] Partition type: 0x%02X\n", part_type);
            
            // FAT32 types: 0x0B (FAT32), 0x0C (FAT32 LBA), 0x1B, 0x1C (hidden)
            if (part_type == 0x0B || part_type == 0x0C || 
                part_type == 0x1B || part_type == 0x1C) {
                // LBA start at offset 8 (little endian 32-bit)
                partition_start = part[8] | (part[9] << 8) | 
                                  (part[10] << 16) | (part[11] << 24);
                printf("[FS] FAT32 partition at sector %lu\n", (unsigned long)partition_start);
            } else if (part_type == 0x00) {
                // No partition, might be superfloppy format
                printf("[FS] No partition table, trying superfloppy\n");
                partition_start = 0;
            } else {
                printf("[FS] Unknown partition type 0x%02X\n", part_type);
                // Try anyway - might be formatted without proper type
            }
        } else {
            printf("[FS] Boot sector at sector 0 (no MBR)\n");
        }
    } else {
        printf("[FS] No boot signature found!\n");
        // Dump first 16 bytes for debugging
        printf("[FS] Sector 0: ");
        for (int i = 0; i < 16; i++) {
            printf("%02X ", vol.sector_buf[i]);
        }
        printf("\n");
        return MIMIC_ERR_CORRUPT;
    }
    
    // Read actual boot sector if partition_start != 0
    if (partition_start != 0) {
        err = fat32_read_sector(partition_start);
        if (err != MIMIC_OK) {
            printf("[FS] Failed to read boot sector at %lu\n", (unsigned long)partition_start);
            return err;
        }
    }
    
    Fat32BPB* bpb = (Fat32BPB*)vol.sector_buf;
    
    // Dump BPB info for debugging
    printf("[FS] BPB: bytes/sector=%u, sectors/cluster=%u\n", 
           bpb->bytes_per_sector, bpb->sectors_per_cluster);
    printf("[FS] BPB: reserved=%u, FATs=%u, FAT size=%lu\n",
           bpb->reserved_sectors, bpb->num_fats, (unsigned long)bpb->fat_size_32);
    printf("[FS] BPB: root cluster=%lu, total sectors=%lu\n",
           (unsigned long)bpb->root_cluster, (unsigned long)bpb->total_sectors_32);
    
    // Check for valid FAT32
    if (bpb->bytes_per_sector != 512) {
        printf("[FS] Invalid bytes per sector: %u\n", bpb->bytes_per_sector);
        return MIMIC_ERR_CORRUPT;
    }
    
    if (bpb->sectors_per_cluster == 0 || bpb->fat_size_32 == 0) {
        printf("[FS] Invalid BPB values\n");
        return MIMIC_ERR_CORRUPT;
    }
    
    vol.partition_start = partition_start;
    vol.sectors_per_cluster = bpb->sectors_per_cluster;
    vol.bytes_per_cluster = 512 * vol.sectors_per_cluster;
    vol.fat_start_sector = partition_start + bpb->reserved_sectors;
    vol.fat_sectors = bpb->fat_size_32;
    vol.root_cluster = bpb->root_cluster;
    vol.data_start_sector = vol.fat_start_sector + (bpb->num_fats * vol.fat_sectors);
    vol.total_clusters = (bpb->total_sectors_32 - (vol.data_start_sector - partition_start)) / vol.sectors_per_cluster;
    
    printf("[FS] FAT start: %lu, Data start: %lu, Clusters: %lu\n",
           (unsigned long)vol.fat_start_sector,
           (unsigned long)vol.data_start_sector,
           (unsigned long)vol.total_clusters);
    
    // Init file handles
    for (int i = 0; i < MIMIC_MAX_FILES; i++) {
        files[i].open = false;
    }
    
    strcpy(current_dir, "/");
    
    printf("[FS] FAT32 mounted OK\n");
    return MIMIC_OK;
}

void mimic_fat32_unmount(void) {
    fat32_flush_cache();
    vol.initialized = false;
}

bool mimic_fat32_mounted(void) {
    return vol.initialized;
}

// ============================================================================
// PATH RESOLUTION
// ============================================================================

static void fat32_name_to_83(const char* name, char* name83) {
    memset(name83, ' ', 11);
    
    int i = 0, j = 0;
    while (name[i] && name[i] != '.' && j < 8) {
        char c = name[i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        name83[j++] = c;
    }
    
    if (name[i] == '.') {
        i++;
        j = 8;
        while (name[i] && j < 11) {
            char c = name[i++];
            if (c >= 'a' && c <= 'z') c -= 32;
            name83[j++] = c;
        }
    }
}

static int fat32_resolve_path(const char* path, uint32_t* out_cluster, Fat32DirEntry* out_entry) {
    uint32_t cluster = vol.root_cluster;
    
    if (path[0] == '/') path++;
    if (path[0] == '\0') {
        if (out_cluster) *out_cluster = vol.root_cluster;
        return MIMIC_OK;
    }
    
    char component[256];
    char name83[11];
    
    while (*path) {
        // Extract component
        int len = 0;
        while (*path && *path != '/' && len < 255) {
            component[len++] = *path++;
        }
        component[len] = '\0';
        if (*path == '/') path++;
        
        fat32_name_to_83(component, name83);
        
        // Search directory
        bool found = false;
        uint32_t cur_cluster = cluster;
        
        while (cur_cluster < FAT32_EOC) {
            uint32_t sector = fat32_cluster_to_sector(cur_cluster);
            
            for (uint32_t s = 0; s < vol.sectors_per_cluster; s++) {
                if (fat32_read_sector(sector + s) != MIMIC_OK) {
                    return MIMIC_ERR_IO;
                }
                
                Fat32DirEntry* entries = (Fat32DirEntry*)vol.sector_buf;
                
                for (int e = 0; e < 16; e++) {
                    if (entries[e].name[0] == 0x00) break;
                    if (entries[e].name[0] == 0xE5) continue;
                    if (entries[e].attr == FAT_ATTR_LFN) continue;
                    
                    if (memcmp(entries[e].name, name83, 8) == 0 &&
                        memcmp(entries[e].ext, name83 + 8, 3) == 0) {
                        
                        cluster = ((uint32_t)entries[e].fst_clus_hi << 16) | entries[e].fst_clus_lo;
                        
                        if (out_entry) *out_entry = entries[e];
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
            if (found) break;
            
            cur_cluster = fat32_get_fat_entry(cur_cluster);
        }
        
        if (!found) return MIMIC_ERR_NOENT;
    }
    
    if (out_cluster) *out_cluster = cluster;
    return MIMIC_OK;
}

// ============================================================================
// FILE OPERATIONS
// ============================================================================

int mimic_fopen(const char* path, uint8_t mode) {
    printf("[FS] fopen: path='%s' mode=0x%02X\n", path, mode);
    
    // Find free handle
    int fd = -1;
    for (int i = 0; i < MIMIC_MAX_FILES; i++) {
        if (!files[i].open) {
            fd = i;
            break;
        }
    }
    if (fd < 0) return MIMIC_ERR_NOMEM;
    
    MimicFile* f = &files[fd];
    memset(f, 0, sizeof(MimicFile));
    
    Fat32DirEntry entry;
    int err = fat32_resolve_path(path, &f->first_cluster, &entry);
    
    printf("[FS] resolve_path returned: err=%d first_clus=%lu size=%lu\n", 
           err, (unsigned long)f->first_cluster, (unsigned long)entry.file_size);
    
    if (err == MIMIC_ERR_NOENT && (mode & MIMIC_FILE_CREATE)) {
        // Extract parent directory and filename
        char parent[MIMIC_MAX_PATH];
        char filename[64];
        strncpy(parent, path, MIMIC_MAX_PATH - 1);
        parent[MIMIC_MAX_PATH - 1] = '\0';
        
        // Find last slash
        char* last_slash = strrchr(parent, '/');
        if (last_slash == NULL) {
            // No slash - use root dir
            strcpy(parent, "/");
            strncpy(filename, path, 63);
        } else if (last_slash == parent) {
            // File in root directory
            strcpy(parent, "/");
            strncpy(filename, last_slash + 1, 63);
        } else {
            *last_slash = '\0';
            strncpy(filename, last_slash + 1, 63);
        }
        filename[63] = '\0';
        
        // Resolve parent directory
        uint32_t parent_cluster;
        Fat32DirEntry parent_entry;
        
        if (strcmp(parent, "/") == 0) {
            parent_cluster = vol.root_cluster;
        } else {
            err = fat32_resolve_path(parent, &parent_cluster, &parent_entry);
            if (err != MIMIC_OK) return err;
            if (!(parent_entry.attr & FAT_ATTR_DIRECTORY)) return MIMIC_ERR_NOTDIR;
        }
        
        // Create the file
        err = fat32_create_file(parent_cluster, filename, &entry, 
                                &f->dir_cluster, &f->dir_entry_idx);
        if (err != MIMIC_OK) return err;
        
        f->first_cluster = 0;  // New file has no clusters yet
    } else if (err != MIMIC_OK) {
        return err;
    }
    
    f->open = true;
    f->mode = mode;
    f->file_size = entry.file_size;
    f->position = 0;
    f->current_cluster = f->first_cluster;
    f->cluster_offset = 0;
    f->is_dir = (entry.attr & FAT_ATTR_DIRECTORY) != 0;
    strncpy(f->path, path, MIMIC_MAX_PATH - 1);
    
    if (mode & MIMIC_FILE_APPEND) {
        mimic_fseek(fd, 0, MIMIC_SEEK_END);
    }
    
    if (mode & MIMIC_FILE_TRUNC) {
        f->file_size = 0;
        // TODO: Free clusters
    }
    
    printf("[FS] fopen success: fd=%d first_clus=%lu size=%lu\n",
           fd, (unsigned long)f->first_cluster, (unsigned long)f->file_size);
    
    return fd;
}

int mimic_fclose(int fd) {
    if (fd < 0 || fd >= MIMIC_MAX_FILES) return MIMIC_ERR_INVAL;
    if (!files[fd].open) return MIMIC_ERR_INVAL;
    
    MimicFile* f = &files[fd];
    
    printf("[FS] fclose: fd=%d size=%lu first_clus=%lu\n", 
           fd, (unsigned long)f->file_size, (unsigned long)f->first_cluster);
    
    // Update directory entry if file was written to
    if ((f->mode & MIMIC_FILE_WRITE) && f->dir_cluster != 0) {
        printf("[FS] Updating dir entry: dir_clus=%lu idx=%lu\n",
               (unsigned long)f->dir_cluster, (unsigned long)f->dir_entry_idx);
        
        // Calculate sector and offset of directory entry
        uint32_t entry_in_cluster = f->dir_entry_idx % (vol.sectors_per_cluster * 16);
        uint32_t sector_in_cluster = entry_in_cluster / 16;
        uint32_t entry_in_sector = entry_in_cluster % 16;
        
        uint32_t sector = fat32_cluster_to_sector(f->dir_cluster) + sector_in_cluster;
        
        printf("[FS] Dir sector=%lu entry=%lu\n", 
               (unsigned long)sector, (unsigned long)entry_in_sector);
        
        if (fat32_read_sector(sector) == MIMIC_OK) {
            Fat32DirEntry* entries = (Fat32DirEntry*)vol.sector_buf;
            Fat32DirEntry* entry = &entries[entry_in_sector];
            
            // Update file size and first cluster
            entry->file_size = f->file_size;
            entry->fst_clus_hi = (f->first_cluster >> 16) & 0xFFFF;
            entry->fst_clus_lo = f->first_cluster & 0xFFFF;
            
            printf("[FS] Updated: size=%lu clus=%lu\n",
                   (unsigned long)entry->file_size, 
                   (unsigned long)f->first_cluster);
            
            fat32_cache_dirty();
        } else {
            printf("[FS] FAIL: Could not read directory sector\n");
        }
    }
    
    fat32_flush_cache();
    files[fd].open = false;
    return MIMIC_OK;
}

int mimic_fread(int fd, void* buf, size_t size) {
    if (fd < 0 || fd >= MIMIC_MAX_FILES) return MIMIC_ERR_INVAL;
    MimicFile* f = &files[fd];
    if (!f->open) return MIMIC_ERR_INVAL;
    if (!(f->mode & MIMIC_FILE_READ)) return MIMIC_ERR_PERM;
    
    printf("[FS] fread: fd=%d size=%u pos=%lu/%lu clus=%lu\n",
           fd, (unsigned)size, (unsigned long)f->position, 
           (unsigned long)f->file_size, (unsigned long)f->current_cluster);
    
    uint8_t* out = (uint8_t*)buf;
    size_t bytes_read = 0;
    
    while (bytes_read < size && f->position < f->file_size) {
        if (f->current_cluster == 0 || f->current_cluster >= FAT32_EOC) break;
        
        uint32_t sector_in_cluster = f->cluster_offset / 512;
        uint32_t offset_in_sector = f->cluster_offset % 512;
        uint32_t sector = fat32_cluster_to_sector(f->current_cluster) + sector_in_cluster;
        
        if (fat32_read_sector(sector) != MIMIC_OK) {
            return bytes_read > 0 ? (int)bytes_read : MIMIC_ERR_IO;
        }
        
        uint32_t bytes_in_sector = 512 - offset_in_sector;
        uint32_t bytes_in_file = f->file_size - f->position;
        uint32_t bytes_to_copy = size - bytes_read;
        
        if (bytes_to_copy > bytes_in_sector) bytes_to_copy = bytes_in_sector;
        if (bytes_to_copy > bytes_in_file) bytes_to_copy = bytes_in_file;
        
        memcpy(out + bytes_read, vol.sector_buf + offset_in_sector, bytes_to_copy);
        
        bytes_read += bytes_to_copy;
        f->position += bytes_to_copy;
        f->cluster_offset += bytes_to_copy;
        
        if (f->cluster_offset >= vol.bytes_per_cluster) {
            f->current_cluster = fat32_get_fat_entry(f->current_cluster);
            f->cluster_offset = 0;
        }
    }
    
    return (int)bytes_read;
}

int mimic_fwrite(int fd, const void* buf, size_t size) {
    if (fd < 0 || fd >= MIMIC_MAX_FILES) return MIMIC_ERR_INVAL;
    MimicFile* f = &files[fd];
    if (!f->open) return MIMIC_ERR_INVAL;
    if (!(f->mode & MIMIC_FILE_WRITE)) return MIMIC_ERR_PERM;
    
    printf("[FS] fwrite: fd=%d size=%u\n", fd, (unsigned)size);
    
    const uint8_t* in = (const uint8_t*)buf;
    size_t bytes_written = 0;
    
    while (bytes_written < size) {
        if (f->current_cluster == 0 || f->current_cluster >= FAT32_EOC) {
            printf("[FS] Allocating new cluster\n");
            uint32_t new_cluster = fat32_alloc_cluster();
            if (new_cluster == 0) {
                printf("[FS] FAIL: No free clusters\n");
                return bytes_written > 0 ? (int)bytes_written : MIMIC_ERR_NOMEM;
            }
            
            printf("[FS] Allocated cluster %lu\n", (unsigned long)new_cluster);
            
            if (f->first_cluster == 0) {
                f->first_cluster = new_cluster;
                printf("[FS] Set first_cluster=%lu\n", (unsigned long)new_cluster);
            }
            
            f->current_cluster = new_cluster;
            f->cluster_offset = 0;
        }
        
        uint32_t sector_in_cluster = f->cluster_offset / 512;
        uint32_t offset_in_sector = f->cluster_offset % 512;
        uint32_t sector = fat32_cluster_to_sector(f->current_cluster) + sector_in_cluster;
        
        if (offset_in_sector != 0 || (size - bytes_written) < 512) {
            if (fat32_read_sector(sector) != MIMIC_OK) {
                return bytes_written > 0 ? (int)bytes_written : MIMIC_ERR_IO;
            }
        }
        
        uint32_t bytes_in_sector = 512 - offset_in_sector;
        uint32_t bytes_to_copy = size - bytes_written;
        if (bytes_to_copy > bytes_in_sector) bytes_to_copy = bytes_in_sector;
        
        memcpy(vol.sector_buf + offset_in_sector, in + bytes_written, bytes_to_copy);
        fat32_cache_dirty();
        
        bytes_written += bytes_to_copy;
        f->position += bytes_to_copy;
        f->cluster_offset += bytes_to_copy;
        
        if (f->position > f->file_size) {
            f->file_size = f->position;
        }
        
        if (f->cluster_offset >= vol.bytes_per_cluster) {
            uint32_t next = fat32_get_fat_entry(f->current_cluster);
            if (next >= FAT32_EOC) {
                next = fat32_alloc_cluster();
                if (next == 0) {
                    fat32_flush_cache();
                    return (int)bytes_written;
                }
                fat32_set_fat_entry(f->current_cluster, next);
            }
            f->current_cluster = next;
            f->cluster_offset = 0;
        }
    }
    
    fat32_flush_cache();
    return (int)bytes_written;
}

int mimic_fseek(int fd, int32_t offset, int whence) {
    if (fd < 0 || fd >= MIMIC_MAX_FILES) return MIMIC_ERR_INVAL;
    MimicFile* f = &files[fd];
    if (!f->open) return MIMIC_ERR_INVAL;
    
    int32_t new_pos;
    switch (whence) {
        case MIMIC_SEEK_SET: new_pos = offset; break;
        case MIMIC_SEEK_CUR: new_pos = f->position + offset; break;
        case MIMIC_SEEK_END: new_pos = f->file_size + offset; break;
        default: return MIMIC_ERR_INVAL;
    }
    
    if (new_pos < 0) new_pos = 0;
    if (new_pos > (int32_t)f->file_size) new_pos = f->file_size;
    
    f->position = new_pos;
    f->current_cluster = f->first_cluster;
    f->cluster_offset = 0;
    
    uint32_t clusters_to_skip = new_pos / vol.bytes_per_cluster;
    for (uint32_t i = 0; i < clusters_to_skip && f->current_cluster < FAT32_EOC; i++) {
        f->current_cluster = fat32_get_fat_entry(f->current_cluster);
    }
    f->cluster_offset = new_pos % vol.bytes_per_cluster;
    
    return MIMIC_OK;
}

int32_t mimic_ftell(int fd) {
    if (fd < 0 || fd >= MIMIC_MAX_FILES) return MIMIC_ERR_INVAL;
    if (!files[fd].open) return MIMIC_ERR_INVAL;
    return files[fd].position;
}

int32_t mimic_fsize(int fd) {
    if (fd < 0 || fd >= MIMIC_MAX_FILES) return MIMIC_ERR_INVAL;
    if (!files[fd].open) return MIMIC_ERR_INVAL;
    return files[fd].file_size;
}

bool mimic_feof(int fd) {
    if (fd < 0 || fd >= MIMIC_MAX_FILES) return true;
    if (!files[fd].open) return true;
    return files[fd].position >= files[fd].file_size;
}

int mimic_fflush(int fd) {
    (void)fd;
    return fat32_flush_cache();
}

bool mimic_exists(const char* path) {
    Fat32DirEntry entry;
    return fat32_resolve_path(path, NULL, &entry) == MIMIC_OK;
}

bool mimic_is_dir(const char* path) {
    Fat32DirEntry entry;
    if (fat32_resolve_path(path, NULL, &entry) != MIMIC_OK) return false;
    return (entry.attr & FAT_ATTR_DIRECTORY) != 0;
}

int mimic_mkdir(const char* path) {
    (void)path;
    return MIMIC_ERR_NOSYS;  // TODO
}

// ============================================================================
// DIRECTORY LISTING
// ============================================================================

int mimic_opendir(const char* path) {
    return mimic_fopen(path, MIMIC_FILE_READ);
}

int mimic_readdir(int dir_handle, MimicDirEntry* entry) {
    if (dir_handle < 0 || dir_handle >= MIMIC_MAX_FILES) return MIMIC_ERR_INVAL;
    MimicFile* f = &files[dir_handle];
    if (!f->open || !f->is_dir) return MIMIC_ERR_INVAL;
    
    while (f->current_cluster < FAT32_EOC) {
        uint32_t sector = fat32_cluster_to_sector(f->current_cluster);
        uint32_t entry_idx = f->cluster_offset / 32;
        uint32_t sector_offset = entry_idx / 16;
        uint32_t in_sector = entry_idx % 16;
        
        if (sector_offset >= vol.sectors_per_cluster) {
            f->current_cluster = fat32_get_fat_entry(f->current_cluster);
            f->cluster_offset = 0;
            continue;
        }
        
        if (fat32_read_sector(sector + sector_offset) != MIMIC_OK) {
            return MIMIC_ERR_IO;
        }
        
        Fat32DirEntry* de = (Fat32DirEntry*)vol.sector_buf + in_sector;
        f->cluster_offset += 32;
        
        if (de->name[0] == 0x00) return MIMIC_ERR_NOENT;  // End
        if (de->name[0] == 0xE5) continue;  // Deleted
        if (de->attr == FAT_ATTR_LFN) continue;  // Long name
        if (de->attr & FAT_ATTR_VOLUME_ID) continue;
        
        // Convert 8.3 to normal name
        int i = 0, j = 0;
        while (i < 8 && de->name[i] != ' ') {
            entry->name[j++] = de->name[i] >= 'A' && de->name[i] <= 'Z' 
                               ? de->name[i] + 32 : de->name[i];
            i++;
        }
        if (de->ext[0] != ' ') {
            entry->name[j++] = '.';
            i = 0;
            while (i < 3 && de->ext[i] != ' ') {
                entry->name[j++] = de->ext[i] >= 'A' && de->ext[i] <= 'Z'
                                   ? de->ext[i] + 32 : de->ext[i];
                i++;
            }
        }
        entry->name[j] = '\0';
        
        entry->size = de->file_size;
        entry->attr = de->attr;
        entry->is_dir = (de->attr & FAT_ATTR_DIRECTORY) != 0;
        
        return MIMIC_OK;
    }
    
    return MIMIC_ERR_NOENT;
}

int mimic_closedir(int dir_handle) {
    return mimic_fclose(dir_handle);
}

// ============================================================================
// STREAMING I/O (for compiler)
// ============================================================================

int mimic_stream_open(MimicStream* stream, const char* path, uint8_t mode,
                      uint8_t* buffer, uint32_t buf_size) {
    stream->fd = mimic_fopen(path, mode);
    if (stream->fd < 0) return stream->fd;
    
    stream->buffer = buffer;
    stream->buf_size = buf_size;
    stream->buf_pos = 0;
    stream->buf_len = 0;
    stream->eof = false;
    stream->writing = (mode & MIMIC_FILE_WRITE) != 0;
    
    return MIMIC_OK;
}

int mimic_stream_close(MimicStream* stream) {
    if (stream->fd < 0) return MIMIC_ERR_INVAL;
    
    // Flush write buffer
    if (stream->writing && stream->buf_pos > 0) {
        mimic_fwrite(stream->fd, stream->buffer, stream->buf_pos);
    }
    
    int err = mimic_fclose(stream->fd);
    stream->fd = -1;
    return err;
}

int mimic_stream_getc(MimicStream* stream) {
    if (stream->buf_pos >= stream->buf_len) {
        int n = mimic_fread(stream->fd, stream->buffer, stream->buf_size);
        if (n <= 0) {
            stream->eof = true;
            return -1;
        }
        stream->buf_len = n;
        stream->buf_pos = 0;
    }
    
    return stream->buffer[stream->buf_pos++];
}

int mimic_stream_ungetc(MimicStream* stream, int c) {
    if (stream->buf_pos > 0) {
        stream->buf_pos--;
        stream->buffer[stream->buf_pos] = (uint8_t)c;
        return c;
    }
    return -1;
}

int mimic_stream_putc(MimicStream* stream, int c) {
    if (stream->buf_pos >= stream->buf_size) {
        int n = mimic_fwrite(stream->fd, stream->buffer, stream->buf_pos);
        if (n < (int)stream->buf_pos) return MIMIC_ERR_IO;
        stream->buf_pos = 0;
    }
    
    stream->buffer[stream->buf_pos++] = (uint8_t)c;
    return c;
}

int mimic_stream_puts(MimicStream* stream, const char* s) {
    while (*s) {
        if (mimic_stream_putc(stream, *s++) < 0) return MIMIC_ERR_IO;
    }
    return MIMIC_OK;
}

int mimic_stream_read(MimicStream* stream, void* buf, size_t size) {
    uint8_t* out = (uint8_t*)buf;
    size_t total = 0;
    
    // First, use any buffered data
    while (total < size && stream->buf_pos < stream->buf_len) {
        out[total++] = stream->buffer[stream->buf_pos++];
    }
    
    // Then read directly for large requests
    if (size - total >= stream->buf_size) {
        int n = mimic_fread(stream->fd, out + total, size - total);
        if (n > 0) total += n;
    } else {
        // Refill buffer and copy
        while (total < size) {
            int n = mimic_fread(stream->fd, stream->buffer, stream->buf_size);
            if (n <= 0) {
                stream->eof = true;
                break;
            }
            stream->buf_len = n;
            stream->buf_pos = 0;
            
            while (total < size && stream->buf_pos < stream->buf_len) {
                out[total++] = stream->buffer[stream->buf_pos++];
            }
        }
    }
    
    return (int)total;
}

int mimic_stream_write(MimicStream* stream, const void* buf, size_t size) {
    const uint8_t* in = (const uint8_t*)buf;
    size_t total = 0;
    
    while (total < size) {
        // Fill buffer
        while (total < size && stream->buf_pos < stream->buf_size) {
            stream->buffer[stream->buf_pos++] = in[total++];
        }
        
        // Flush if full
        if (stream->buf_pos >= stream->buf_size) {
            int n = mimic_fwrite(stream->fd, stream->buffer, stream->buf_pos);
            if (n < (int)stream->buf_pos) return total > 0 ? (int)total : MIMIC_ERR_IO;
            stream->buf_pos = 0;
        }
    }
    
    return (int)total;
}

bool mimic_stream_eof(MimicStream* stream) {
    return stream->eof;
}

int mimic_stream_flush(MimicStream* stream) {
    if (stream->writing && stream->buf_pos > 0) {
        int n = mimic_fwrite(stream->fd, stream->buffer, stream->buf_pos);
        if (n < (int)stream->buf_pos) return MIMIC_ERR_IO;
        stream->buf_pos = 0;
    }
    return mimic_fflush(stream->fd);
}

// ============================================================================
// FS INFO
// ============================================================================

int mimic_fs_info(MimicFSInfo* info) {
    if (!vol.initialized) return MIMIC_ERR_IO;
    
    info->sector_size = 512;
    info->cluster_size = vol.bytes_per_cluster;
    info->total_clusters = vol.total_clusters;
    info->total_bytes = (uint64_t)vol.total_clusters * vol.bytes_per_cluster;
    
    // Count free clusters (slow!)
    uint32_t free_count = 0;
    for (uint32_t c = 2; c < vol.total_clusters + 2; c++) {
        if (fat32_get_fat_entry(c) == FAT32_FREE) free_count++;
    }
    
    info->free_clusters = free_count;
    info->free_bytes = (uint64_t)free_count * vol.bytes_per_cluster;
    info->used_bytes = info->total_bytes - info->free_bytes;
    
    return MIMIC_OK;
}
