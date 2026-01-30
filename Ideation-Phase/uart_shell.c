/**
 * UART Shell Implementation
 * 
 * Simple command-line interface for MimiC
 */

#include "uart_shell.h"
#include "../compiler/tcc_port.h"
#include "../compiler/symbol_table.h"
#include "../filesystem/sd_card.h"
#include "../runtime/executor.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"

#define SHELL_BUFFER_SIZE 256

/**
 * Print shell prompt
 */
static void print_prompt(void) {
    printf("mimic> ");
    fflush(stdout);
}

/**
 * Trim whitespace from string
 */
static char *trim(char *str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    *(end + 1) = 0;
    
    return str;
}

/**
 * Command: help
 */
static void cmd_help(void) {
    printf("\nMimiC Commands:\n");
    printf("  help              - Show this help message\n");
    printf("  compile <file>    - Compile C file from SD card\n");
    printf("  run               - Run last compiled program\n");
    printf("  stop              - Stop running program\n");
    printf("  symbols           - Dump symbol table\n");
    printf("  ls [dir]          - List files on SD card\n");
    printf("  cat <file>        - Show file contents\n");
    printf("  mem               - Show memory usage\n");
    printf("  reset             - Reset the device\n");
    printf("\n");
}

/**
 * Command: compile
 */
static void cmd_compile(const char *filename) {
    if (!filename || strlen(filename) == 0) {
        printf("Usage: compile <file>\n");
        return;
    }
    
    printf("Compiling: %s\n", filename);
    
    CompileResult result;
    int ret = tcc_compile_file(filename, &result);
    
    if (ret == 0 && result.success) {
        printf("Compilation successful!\n");
        printf("  Code size: %zu bytes\n", result.code_size);
        printf("  Entry point: 0x%08lx\n", (unsigned long)result.entry_point);
    } else {
        printf("Compilation failed: %s\n", result.error_msg);
    }
}

/**
 * Command: symbols
 */
static void cmd_symbols(void) {
    symbol_table_dump();
}

/**
 * Command: mem
 */
static void cmd_mem(void) {
    printf("\nMemory Usage:\n");
    printf("  Free heap: %d bytes\n", xPortGetFreeHeapSize());
    
    TCCMemStats stats;
    tcc_get_mem_stats(&stats);
    printf("  TCC allocated: %zu bytes\n", stats.total_allocated);
    printf("  TCC current: %zu bytes\n", stats.current_usage);
    printf("  TCC peak: %zu bytes\n", stats.peak_usage);
    printf("\n");
}

/**
 * Command: ls
 */
static int ls_callback(const char *filename, int is_dir, size_t size) {
    if (is_dir) {
        printf("  [DIR]  %s\n", filename);
    } else {
        printf("  [FILE] %-30s  %zu bytes\n", filename, size);
    }
    return 1;
}

static void cmd_ls(const char *path) {
    if (!path || strlen(path) == 0) {
        path = "/";
    }
    
    printf("Listing: %s\n", path);
    sd_list_directory(path, ls_callback);
}

/**
 * Command: cat
 */
static void cmd_cat(const char *filename) {
    if (!filename || strlen(filename) == 0) {
        printf("Usage: cat <file>\n");
        return;
    }
    
    char *buffer = NULL;
    size_t size = 0;
    
    if (sd_read_file(filename, &buffer, &size) == 0) {
        printf("\n--- %s ---\n", filename);
        printf("%s", buffer);
        printf("\n--- End of file ---\n\n");
        vPortFree(buffer);
    } else {
        printf("Failed to read file: %s\n", filename);
    }
}

/**
 * Command: reset
 */
static void cmd_reset(void) {
    printf("Resetting device...\n");
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Watchdog reset
    watchdog_enable(1, 1);
    while (1) {
        tight_loop_contents();
    }
}

/**
 * Process command
 */
static void process_command(char *line) {
    line = trim(line);
    
    if (strlen(line) == 0) {
        return;
    }
    
    // Parse command and arguments
    char *cmd = strtok(line, " \t");
    char *arg = strtok(NULL, "");
    if (arg) {
        arg = trim(arg);
    }
    
    // Execute command
    if (strcmp(cmd, "help") == 0) {
        cmd_help();
    } else if (strcmp(cmd, "compile") == 0) {
        cmd_compile(arg);
    } else if (strcmp(cmd, "symbols") == 0) {
        cmd_symbols();
    } else if (strcmp(cmd, "mem") == 0) {
        cmd_mem();
    } else if (strcmp(cmd, "ls") == 0) {
        cmd_ls(arg);
    } else if (strcmp(cmd, "cat") == 0) {
        cmd_cat(arg);
    } else if (strcmp(cmd, "reset") == 0) {
        cmd_reset();
    } else {
        printf("Unknown command: %s (type 'help' for available commands)\n", cmd);
    }
}

/**
 * Shell task
 */
void shell_task(void *params) {
    (void)params;
    
    char buffer[SHELL_BUFFER_SIZE];
    int pos = 0;
    
    printf("\nMimiC Shell v0.1\n");
    printf("Type 'help' for available commands\n\n");
    
    print_prompt();
    
    while (1) {
        int c = getchar_timeout_us(0);
        
        if (c == PICO_ERROR_TIMEOUT) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        
        if (c == '\r' || c == '\n') {
            printf("\n");
            buffer[pos] = '\0';
            
            if (pos > 0) {
                process_command(buffer);
                pos = 0;
            }
            
            print_prompt();
        } else if (c == '\b' || c == 127) {
            // Backspace
            if (pos > 0) {
                pos--;
                printf("\b \b");
                fflush(stdout);
            }
        } else if (c >= 32 && c < 127) {
            // Printable character
            if (pos < SHELL_BUFFER_SIZE - 1) {
                buffer[pos++] = c;
                putchar(c);
                fflush(stdout);
            }
        }
    }
}
