/**
 * MimiC - On-Device C Compiler for Microcontrollers
 * 
 * Main entry point and initialization
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/watchdog.h"

#include "FreeRTOS.h"
#include "task.h"

#include "compiler/tcc_port.h"
#include "filesystem/sd_card.h"
#include "runtime/executor.h"
#include "shell/uart_shell.h"

#define LED_PIN 25

/* Task priorities */
#define PRIORITY_COMPILER  (tskIDLE_PRIORITY + 2)
#define PRIORITY_SHELL     (tskIDLE_PRIORITY + 1)
#define PRIORITY_USER_CODE (tskIDLE_PRIORITY + 1)

/**
 * Blink LED to show we're alive
 */
static void heartbeat_task(void *params) {
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    while (1) {
        gpio_put(LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_put(LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(900));
    }
}

/**
 * Compiler task - waits for compilation requests
 */
static void compiler_task(void *params) {
    printf("[MimiC] Compiler task started\n");
    
    // Initialize TCC
    if (tcc_init() != 0) {
        printf("[MimiC] ERROR: Failed to initialize TCC\n");
        vTaskDelete(NULL);
        return;
    }
    
    printf("[MimiC] TCC initialized successfully\n");
    printf("[MimiC] Ready to compile programs from SD card\n");
    
    while (1) {
        // Wait for compilation requests from shell
        // TODO: Implement message queue for compile requests
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * Initialize all subsystems
 */
static int init_subsystems(void) {
    // Initialize stdio
    stdio_init_all();
    sleep_ms(2000); // Give USB time to enumerate
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║  MimiC - On-Device C Compiler for Microcontrollers   ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("Hardware: RP2040\n");
    printf("SDK Version: %s\n", PICO_SDK_VERSION_STRING);
    printf("FreeRTOS Version: %s\n", tskKERNEL_VERSION_NUMBER);
    printf("\n");
    
    // Initialize SD card
    printf("[MimiC] Initializing SD card...\n");
    if (sd_card_init() != 0) {
        printf("[MimiC] WARNING: SD card not detected\n");
        printf("[MimiC] Insert SD card and reset device\n");
        // Don't fail - allow shell to work without SD
    } else {
        printf("[MimiC] SD card mounted successfully\n");
    }
    
    // Load symbol table
    printf("[MimiC] Loading SDK symbol table...\n");
    symbol_table_init();
    printf("[MimiC] Symbol table loaded (%d symbols)\n", symbol_table_count());
    
    printf("\n[MimiC] Initialization complete\n");
    printf("[MimiC] Type 'help' for available commands\n\n");
    
    return 0;
}

/**
 * Main entry point
 */
int main() {
    // Initialize all subsystems
    if (init_subsystems() != 0) {
        printf("[MimiC] FATAL: Initialization failed\n");
        while (1) {
            tight_loop_contents();
        }
    }
    
    // Create FreeRTOS tasks
    xTaskCreate(heartbeat_task, "Heartbeat", 128, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(compiler_task, "Compiler", 2048, NULL, PRIORITY_COMPILER, NULL);
    xTaskCreate(shell_task, "Shell", 1024, NULL, PRIORITY_SHELL, NULL);
    
    // Start scheduler
    printf("[MimiC] Starting FreeRTOS scheduler...\n\n");
    vTaskStartScheduler();
    
    // Should never reach here
    printf("[MimiC] FATAL: Scheduler returned\n");
    while (1) {
        tight_loop_contents();
    }
    
    return 0;
}

/**
 * FreeRTOS malloc failed hook
 */
void vApplicationMallocFailedHook(void) {
    printf("[MimiC] FATAL: malloc failed - out of memory\n");
    printf("[MimiC] Available heap: %d bytes\n", xPortGetFreeHeapSize());
    
    // Blink rapidly to indicate error
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    while (1) {
        gpio_put(LED_PIN, 1);
        sleep_ms(50);
        gpio_put(LED_PIN, 0);
        sleep_ms(50);
    }
}

/**
 * FreeRTOS stack overflow hook
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    printf("[MimiC] FATAL: Stack overflow in task '%s'\n", pcTaskName);
    
    // Blink rapidly to indicate error
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    while (1) {
        gpio_put(LED_PIN, 1);
        sleep_ms(100);
        gpio_put(LED_PIN, 0);
        sleep_ms(100);
    }
}
