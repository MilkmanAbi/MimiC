/**
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║  MimiC - Self-Hosted C Compiler for RP2040/RP2350                         ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  Main Entry Point                                                          ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

#include "mimic.h"
#include "mimic_fat32.h"

// ============================================================================
// BANNER
// ============================================================================

static void print_banner(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  __  __ _           _  ____                                  ║\n");
    printf("║ |  \\/  (_)_ __ ___ (_)/ ___|   Self-Hosted C Compiler        ║\n");
    printf("║ | |\\/| | | '_ ` _ \\| | |       for RP2040/RP2350             ║\n");
    printf("║ | |  | | | | | | | | | |___                                  ║\n");
    printf("║ |_|  |_|_|_| |_| |_|_|\\____|   v1.0.0-alpha                   ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

// ============================================================================
// SHELL COMMANDS
// ============================================================================

#define CMD_BUF_SIZE    256
#define MAX_ARGS        16

typedef struct {
    const char* name;
    const char* help;
    int (*handler)(int argc, char* argv[]);
} Command;

static int cmd_help(int argc, char* argv[]);
static int cmd_ls(int argc, char* argv[]);
static int cmd_cat(int argc, char* argv[]);
static int cmd_cc(int argc, char* argv[]);
static int cmd_run(int argc, char* argv[]);
static int cmd_mem(int argc, char* argv[]);
static int cmd_tasks(int argc, char* argv[]);
static int cmd_info(int argc, char* argv[]);
static int cmd_test(int argc, char* argv[]);

// From mimic_compiler.c
int mimic_compile(const char* input, const char* output);
const char* mimic_compile_error(void);

static const Command commands[] = {
    {"help",    "Show this help message",           cmd_help},
    {"ls",      "List directory contents",          cmd_ls},
    {"cat",     "Display file contents",            cmd_cat},
    {"cc",      "Compile C source file",            cmd_cc},
    {"run",     "Load and run .mimi binary",        cmd_run},
    {"mem",     "Show memory usage",                cmd_mem},
    {"tasks",   "Show running tasks",               cmd_tasks},
    {"info",    "Show system information",          cmd_info},
    {"test",    "Run compiler tests",               cmd_test},
    {NULL, NULL, NULL}
};

static int cmd_help(int argc, char* argv[]) {
    (void)argc; (void)argv;
    printf("\nAvailable commands:\n");
    for (const Command* cmd = commands; cmd->name; cmd++) {
        printf("  %-10s %s\n", cmd->name, cmd->help);
    }
    printf("\n");
    return 0;
}

static int cmd_ls(int argc, char* argv[]) {
    const char* path = argc > 1 ? argv[1] : "/";
    
    int dir = mimic_opendir(path);
    if (dir < 0) {
        printf("Error: Cannot open directory '%s'\n", path);
        return -1;
    }
    
    MimicDirEntry entry;
    printf("\n");
    while (mimic_readdir(dir, &entry) == MIMIC_OK) {
        if (entry.is_dir) {
            printf("  [DIR]  %s\n", entry.name);
        } else {
            printf("  %6lu %s\n", (unsigned long)entry.size, entry.name);
        }
    }
    printf("\n");
    
    mimic_closedir(dir);
    return 0;
}

static int cmd_cat(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: cat <filename>\n");
        return -1;
    }
    
    int fd = mimic_fopen(argv[1], MIMIC_FILE_READ);
    if (fd < 0) {
        printf("Error: Cannot open '%s'\n", argv[1]);
        return -1;
    }
    
    char buf[128];
    int n;
    printf("\n");
    while ((n = mimic_fread(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        printf("%s", buf);
    }
    printf("\n");
    
    mimic_fclose(fd);
    return 0;
}

static int cmd_cc(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: cc <source.c> [output.mimi]\n");
        return -1;
    }
    
    const char* input = argv[1];
    char output[64];
    
    if (argc >= 3) {
        strncpy(output, argv[2], 63);
    } else {
        // Generate output name
        strncpy(output, input, 59);
        char* dot = strrchr(output, '.');
        if (dot) strcpy(dot, ".mimi");
        else strcat(output, ".mimi");
    }
    
    int err = mimic_compile(input, output);
    if (err == MIMIC_OK) {
        printf("Compiled: %s -> %s\n", input, output);
    } else {
        printf("Error: %s\n", mimic_compile_error());
    }
    
    return err;
}

static int cmd_run(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: run <program.mimi>\n");
        return -1;
    }
    
    printf("Loading '%s'...\n", argv[1]);
    
    int task_id = mimic_task_load(argv[1], 10);
    if (task_id < 0) {
        printf("Error: Failed to load program (%d)\n", task_id);
        return -1;
    }
    
    printf("Started task %d\n", task_id);
    return 0;
}

static int cmd_mem(int argc, char* argv[]) {
    (void)argc; (void)argv;
    mimic_dump_memory();
    return 0;
}

static int cmd_tasks(int argc, char* argv[]) {
    (void)argc; (void)argv;
    mimic_dump_tasks();
    return 0;
}

static int cmd_info(int argc, char* argv[]) {
    (void)argc; (void)argv;
    printf("\n=== SYSTEM INFO ===\n");
    printf("Chip:        " MIMIC_CHIP_NAME "\n");
    printf("Uptime:      %lu ms\n", (unsigned long)mimic_get_uptime_ms());
    printf("Free memory: %lu bytes\n", (unsigned long)mimic_get_free_memory());
    printf("Tasks:       %lu\n", (unsigned long)mimic_get_task_count());
    
    if (mimic_fat32_mounted()) {
        MimicFSInfo info;
        if (mimic_fs_info(&info) == MIMIC_OK) {
            printf("\n=== FILESYSTEM ===\n");
            printf("Total:       %llu MB\n", info.total_bytes / (1024*1024));
            printf("Free:        %llu MB\n", info.free_bytes / (1024*1024));
            printf("Cluster:     %lu bytes\n", (unsigned long)info.cluster_size);
        }
    }
    printf("\n");
    return 0;
}

static int cmd_test(int argc, char* argv[]) {
    (void)argc; (void)argv;
    
    printf("\n=== MimiC COMPILER TEST ===\n\n");
    
    // Test 1: Create test file
    printf("[1] Creating test file...\n");
    const char* test_code = 
        "int add(int a, int b) {\n"
        "    return a + b;\n"
        "}\n"
        "\n"
        "int main() {\n"
        "    int x = 10;\n"
        "    int y = 32;\n"
        "    return add(x, y);\n"
        "}\n";
    
    int fd = mimic_fopen("/test.c", MIMIC_FILE_WRITE | MIMIC_FILE_CREATE);
    if (fd < 0) {
        printf("    FAIL: Cannot create /test.c (err=%d)\n", fd);
        return -1;
    }
    
    int written = mimic_fwrite(fd, test_code, strlen(test_code));
    mimic_fclose(fd);
    printf("    OK: Wrote %d bytes to /test.c\n", written);
    
    // Verify file
    fd = mimic_fopen("/test.c", MIMIC_FILE_READ);
    if (fd < 0) {
        printf("    FAIL: Cannot read back /test.c\n");
        return -1;
    }
    char buf[64];
    int n = mimic_fread(fd, buf, 63);
    buf[n > 0 ? n : 0] = 0;
    mimic_fclose(fd);
    printf("    Verify: First %d bytes: \"%.20s...\"\n", n, buf);
    
    // Test 2: Compile
    printf("\n[2] Compiling...\n");
    int err = mimic_compile("/test.c", "/test.mimi");
    
    if (err == MIMIC_OK) {
        printf("    PASS: Compilation successful!\n");
        
        // Check output file
        fd = mimic_fopen("/test.mimi", MIMIC_FILE_READ);
        if (fd >= 0) {
            MimiHeader header;
            mimic_fread(fd, &header, sizeof(header));
            mimic_fclose(fd);
            
            printf("    Output: .text=%lu bytes, .bss=%lu bytes\n",
                   (unsigned long)header.text_size,
                   (unsigned long)header.bss_size);
        }
    } else {
        printf("    FAIL: %s\n", mimic_compile_error());
    }
    
    printf("\n");
    return 0;
}

// ============================================================================
// SHELL
// ============================================================================

static void parse_and_execute(char* line) {
    char* argv[MAX_ARGS];
    int argc = 0;
    
    char* token = strtok(line, " \t\n");
    while (token && argc < MAX_ARGS) {
        argv[argc++] = token;
        token = strtok(NULL, " \t\n");
    }
    
    if (argc == 0) return;
    
    for (const Command* cmd = commands; cmd->name; cmd++) {
        if (strcmp(argv[0], cmd->name) == 0) {
            cmd->handler(argc, argv);
            return;
        }
    }
    
    printf("Unknown command: %s (type 'help' for commands)\n", argv[0]);
}

static void shell_loop(void) {
    char buf[CMD_BUF_SIZE];
    int pos = 0;
    
    printf("mimic> ");
    
    while (true) {
        int c = getchar();
        if (c == PICO_ERROR_TIMEOUT) {
            tight_loop_contents();
            continue;
        }
        
        if (c == '\r' || c == '\n') {
            printf("\n");
            buf[pos] = '\0';
            if (pos > 0) {
                parse_and_execute(buf);
            }
            pos = 0;
            printf("mimic> ");
        } else if (c == '\b' || c == 127) {
            if (pos > 0) {
                pos--;
                printf("\b \b");
            }
        } else if (c >= 32 && c < 127 && pos < CMD_BUF_SIZE - 1) {
            buf[pos++] = c;
            putchar(c);
        }
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    stdio_init_all();
    
    // Wait for USB connection
    sleep_ms(2000);
    
    print_banner();
    
    // Initialize kernel
    mimic_kernel_init();
    
    // Mount filesystem
    printf("[FS] Mounting SD card...\n");
    int err = mimic_fat32_mount();
    if (err == MIMIC_OK) {
        printf("[FS] SD card mounted successfully!\n");
        
        // Show some info
        MimicFSInfo info;
        if (mimic_fs_info(&info) == MIMIC_OK) {
            printf("[FS] Total: %llu MB, Free: %llu MB\n", 
                   info.total_bytes / (1024*1024),
                   info.free_bytes / (1024*1024));
        }
    } else {
        printf("[FS] Mount failed (error %d)\n", err);
        printf("[FS] Check SD card and wiring:\n");
        printf("     CS=%d, MOSI=%d, MISO=%d, SCK=%d\n",
               MIMIC_SD_CS, MIMIC_SD_MOSI, MIMIC_SD_MISO, MIMIC_SD_SCK);
    }
    
    // Start shell
    printf("\nType 'help' for commands\n\n");
    shell_loop();
    
    return 0;
}
