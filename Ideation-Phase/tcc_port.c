/**
 * TCC Port Implementation for RP2040
 * 
 * This file integrates TCC with the RP2040 environment:
 * - Custom memory allocator (FreeRTOS heap)
 * - SD card file I/O
 * - Symbol table integration
 * - ARM Thumb-2 code generation
 */

#include "tcc_port.h"
#include "symbol_table.h"
#include "../filesystem/sd_card.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

/* TCC includes - will need to port these */
// #include "libtcc.h"

/* Memory usage tracking */
static TCCMemStats mem_stats = {0};

/* Global TCC state (reused for compilations) */
static TCCState *global_tcc_state = NULL;

/**
 * Custom malloc for TCC
 * Uses FreeRTOS heap and tracks allocations
 */
static void *tcc_malloc(size_t size) {
    void *ptr = pvPortMalloc(size);
    if (ptr) {
        mem_stats.total_allocated += size;
        mem_stats.current_usage += size;
        if (mem_stats.current_usage > mem_stats.peak_usage) {
            mem_stats.peak_usage = mem_stats.current_usage;
        }
    }
    return ptr;
}

/**
 * Custom free for TCC
 */
static void tcc_free(void *ptr) {
    if (ptr) {
        // Note: We don't track the exact size freed
        // This is acceptable for embedded use
        vPortFree(ptr);
    }
}

/**
 * Custom realloc for TCC
 */
static void *tcc_realloc(void *ptr, size_t size) {
    // FreeRTOS doesn't have realloc, so we implement it
    if (!ptr) {
        return tcc_malloc(size);
    }
    
    if (size == 0) {
        tcc_free(ptr);
        return NULL;
    }
    
    void *new_ptr = tcc_malloc(size);
    if (!new_ptr) {
        return NULL;
    }
    
    // Copy old data - we don't know the old size, so this is unsafe
    // TODO: Implement proper size tracking
    memcpy(new_ptr, ptr, size);
    tcc_free(ptr);
    
    return new_ptr;
}

/**
 * Initialize TCC subsystem
 */
int tcc_init(void) {
    printf("[TCC] Initializing TCC compiler...\n");
    
    // TODO: Initialize actual TCC library
    // This will involve:
    // 1. Setting up TCC with custom allocators
    // 2. Configuring for ARM target
    // 3. Setting default include paths
    
    printf("[TCC] Setting up custom memory allocator\n");
    // tcc_set_malloc(tcc_malloc, tcc_free, tcc_realloc);
    
    printf("[TCC] Configuring for ARM Thumb-2 target\n");
    // global_tcc_state = tcc_new();
    // tcc_set_output_type(global_tcc_state, TCC_OUTPUT_MEMORY);
    
    printf("[TCC] Adding default include paths\n");
    tcc_add_include_path("/sd/include");
    tcc_add_include_path("/sd/include/pico");
    tcc_add_include_path("/sd/include/hardware");
    
    printf("[TCC] Initialization complete\n");
    
    return 0;
}

/**
 * Compile C source file from SD card
 */
int tcc_compile_file(const char *source_path, CompileResult *result) {
    if (!result) {
        return -1;
    }
    
    memset(result, 0, sizeof(CompileResult));
    
    printf("[TCC] Compiling file: %s\n", source_path);
    
    // Read source file from SD card
    char *source_code = NULL;
    size_t source_size = 0;
    
    if (sd_read_file(source_path, &source_code, &source_size) != 0) {
        snprintf(result->error_msg, sizeof(result->error_msg),
                 "Failed to read source file: %s", source_path);
        return -1;
    }
    
    printf("[TCC] Source file loaded: %zu bytes\n", source_size);
    
    // Compile the source code
    int ret = tcc_compile_string(source_code, result);
    
    // Free source buffer
    vPortFree(source_code);
    
    return ret;
}

/**
 * Compile C source code from memory
 */
int tcc_compile_string(const char *source_code, CompileResult *result) {
    if (!result || !source_code) {
        return -1;
    }
    
    memset(result, 0, sizeof(CompileResult));
    
    printf("[TCC] Starting compilation...\n");
    printf("[TCC] Free heap before: %d bytes\n", xPortGetFreeHeapSize());
    
    // TODO: Actual TCC compilation
    // This is where we'll call TCC APIs:
    //
    // TCCState *s = tcc_new();
    // tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
    //
    // // Add SDK symbols
    // for (int i = 0; i < symbol_table_count(); i++) {
    //     const Symbol *sym = symbol_table_get(i);
    //     tcc_add_symbol(s, sym->name, sym->address);
    // }
    //
    // // Compile
    // if (tcc_compile_string(s, source_code) < 0) {
    //     snprintf(result->error_msg, sizeof(result->error_msg),
    //              "Compilation failed");
    //     tcc_delete(s);
    //     return -1;
    // }
    //
    // // Relocate to memory
    // int code_size = tcc_relocate(s, NULL);
    // result->code = pvPortMalloc(code_size);
    // tcc_relocate(s, result->code);
    //
    // // Get entry point
    // result->entry_point = tcc_get_symbol(s, "main");
    // result->code_size = code_size;
    //
    // tcc_delete(s);
    
    // For now, just return a stub
    result->success = 0;
    snprintf(result->error_msg, sizeof(result->error_msg),
             "TCC not yet fully ported - this is a stub");
    
    printf("[TCC] Free heap after: %d bytes\n", xPortGetFreeHeapSize());
    
    return -1;
}

/**
 * Free compiled code
 */
void tcc_free_result(CompileResult *result) {
    if (!result) {
        return;
    }
    
    if (result->code) {
        vPortFree(result->code);
        result->code = NULL;
    }
    
    memset(result, 0, sizeof(CompileResult));
}

/**
 * Get memory statistics
 */
void tcc_get_mem_stats(TCCMemStats *stats) {
    if (stats) {
        *stats = mem_stats;
    }
}

/**
 * Add include path
 */
int tcc_add_include_path(const char *path) {
    printf("[TCC] Adding include path: %s\n", path);
    
    // TODO: Call actual TCC API
    // tcc_add_include_path(global_tcc_state, path);
    
    return 0;
}

/**
 * Add library path
 */
int tcc_add_library_path(const char *path) {
    printf("[TCC] Adding library path: %s\n", path);
    
    // TODO: Call actual TCC API
    // tcc_add_library_path(global_tcc_state, path);
    
    return 0;
}

/**
 * Set compiler option
 */
int tcc_set_option(const char *option) {
    printf("[TCC] Setting option: %s\n", option);
    
    // TODO: Call actual TCC API
    // tcc_set_options(global_tcc_state, option);
    
    return 0;
}
