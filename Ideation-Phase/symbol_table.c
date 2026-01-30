/**
 * Symbol Table Implementation
 * 
 * Contains precompiled SDK function addresses for linking
 */

#include "symbol_table.h"
#include <stdio.h>
#include <string.h>

/* Forward declarations from Pico SDK */
extern void gpio_init(uint gpio);
extern void gpio_set_dir(uint gpio, bool out);
extern void gpio_put(uint gpio, bool value);
extern bool gpio_get(uint gpio);
extern void sleep_ms(uint32_t ms);
extern void stdio_init_all(void);

/* Forward declarations from FreeRTOS */
extern void vTaskDelay(uint32_t ticks);
extern void vTaskStartScheduler(void);

/**
 * Static symbol table
 * 
 * This will eventually be auto-generated from SDK headers,
 * but for now we manually add commonly used functions.
 */
static const Symbol symbol_table[] = {
    /* Pico SDK - GPIO */
    {"gpio_init",           (void*)gpio_init,           "void(uint)"},
    {"gpio_set_dir",        (void*)gpio_set_dir,        "void(uint,bool)"},
    {"gpio_put",            (void*)gpio_put,            "void(uint,bool)"},
    {"gpio_get",            (void*)gpio_get,            "bool(uint)"},
    
    /* Pico SDK - Time */
    {"sleep_ms",            (void*)sleep_ms,            "void(uint32_t)"},
    
    /* Pico SDK - Stdio */
    {"stdio_init_all",      (void*)stdio_init_all,      "void(void)"},
    
    /* FreeRTOS */
    {"vTaskDelay",          (void*)vTaskDelay,          "void(TickType_t)"},
    {"vTaskStartScheduler", (void*)vTaskStartScheduler, "void(void)"},
    
    /* TODO: Add more SDK functions:
     * - I2C (i2c_init, i2c_write_blocking, i2c_read_blocking)
     * - SPI (spi_init, spi_write_blocking, spi_read_blocking)
     * - PWM (pwm_init, pwm_set_duty_cycle, etc.)
     * - ADC (adc_init, adc_read, etc.)
     * - Timers (timer_init, timer_set_alarm, etc.)
     * - DMA (dma_init, dma_start, etc.)
     * - USB (stdio_usb_init, etc.)
     * - FreeRTOS tasks (xTaskCreate, vTaskDelete, etc.)
     * - FreeRTOS queues (xQueueCreate, xQueueSend, etc.)
     */
};

static const size_t symbol_count = sizeof(symbol_table) / sizeof(symbol_table[0]);

/**
 * Initialize symbol table
 */
void symbol_table_init(void) {
    printf("[SymbolTable] Loaded %zu symbols\n", symbol_count);
}

/**
 * Get number of symbols
 */
size_t symbol_table_count(void) {
    return symbol_count;
}

/**
 * Get symbol by index
 */
const Symbol *symbol_table_get(size_t index) {
    if (index >= symbol_count) {
        return NULL;
    }
    return &symbol_table[index];
}

/**
 * Look up symbol by name
 */
const Symbol *symbol_table_lookup(const char *name) {
    if (!name) {
        return NULL;
    }
    
    for (size_t i = 0; i < symbol_count; i++) {
        if (strcmp(symbol_table[i].name, name) == 0) {
            return &symbol_table[i];
        }
    }
    
    return NULL;
}

/**
 * Dump symbol table
 */
void symbol_table_dump(void) {
    printf("\n[SymbolTable] Symbol Table Dump (%zu symbols):\n", symbol_count);
    printf("%-30s %-12s %s\n", "Name", "Address", "Signature");
    printf("────────────────────────────────────────────────────────────────\n");
    
    for (size_t i = 0; i < symbol_count; i++) {
        const Symbol *sym = &symbol_table[i];
        printf("%-30s 0x%08lx   %s\n", 
               sym->name,
               (unsigned long)sym->address,
               sym->signature);
    }
    
    printf("\n");
}
