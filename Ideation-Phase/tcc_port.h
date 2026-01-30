/**
 * TCC (Tiny C Compiler) Port for RP2040
 * 
 * This is a minimal port of TCC for embedded use on the RP2040.
 * We strip out everything except ARM code generation and essential features.
 */

#ifndef TCC_PORT_H
#define TCC_PORT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TCC compilation state */
typedef struct TCCState TCCState;

/* Compilation result */
typedef struct {
    int success;           /* 1 if compilation succeeded */
    void *code;            /* Pointer to compiled code */
    size_t code_size;      /* Size of compiled code in bytes */
    void *entry_point;     /* Entry point (main function) */
    char error_msg[256];   /* Error message if compilation failed */
} CompileResult;

/**
 * Initialize TCC subsystem
 * Must be called once at startup
 * 
 * Returns: 0 on success, -1 on failure
 */
int tcc_init(void);

/**
 * Compile a C source file from SD card
 * 
 * @param source_path Path to .c file on SD card (e.g. "/examples/blink.c")
 * @param result Output compilation result
 * @return 0 on success, -1 on failure
 */
int tcc_compile_file(const char *source_path, CompileResult *result);

/**
 * Compile C source code from memory
 * 
 * @param source_code C source code as string
 * @param result Output compilation result
 * @return 0 on success, -1 on failure
 */
int tcc_compile_string(const char *source_code, CompileResult *result);

/**
 * Free compiled code
 * 
 * @param result Compilation result to free
 */
void tcc_free_result(CompileResult *result);

/**
 * Get TCC memory usage statistics
 */
typedef struct {
    size_t total_allocated;   /* Total bytes allocated */
    size_t peak_usage;         /* Peak memory usage */
    size_t current_usage;      /* Current memory usage */
} TCCMemStats;

void tcc_get_mem_stats(TCCMemStats *stats);

/**
 * Add include path for header files
 * 
 * @param path Path to include directory (e.g. "/sd/include")
 */
int tcc_add_include_path(const char *path);

/**
 * Add library path for linking
 * 
 * @param path Path to library directory
 */
int tcc_add_library_path(const char *path);

/**
 * Set compiler option
 * 
 * @param option Option string (e.g. "-O2", "-g", "-Wall")
 */
int tcc_set_option(const char *option);

#ifdef __cplusplus
}
#endif

#endif /* TCC_PORT_H */
