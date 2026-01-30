/**
 * Symbol Table - Maps SDK function names to addresses
 * 
 * The symbol table is generated at build time and contains
 * all Pico SDK and FreeRTOS functions that user code can call.
 */

#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Symbol entry */
typedef struct {
    const char *name;        /* Function name (e.g. "gpio_init") */
    void *address;           /* Function address in flash */
    const char *signature;   /* Function signature for type checking */
} Symbol;

/**
 * Initialize symbol table
 * Loads SDK symbols from flash
 */
void symbol_table_init(void);

/**
 * Get number of symbols in table
 */
size_t symbol_table_count(void);

/**
 * Get symbol by index
 * 
 * @param index Symbol index (0 to count-1)
 * @return Pointer to symbol, or NULL if index out of range
 */
const Symbol *symbol_table_get(size_t index);

/**
 * Look up symbol by name
 * 
 * @param name Function name to look up
 * @return Pointer to symbol, or NULL if not found
 */
const Symbol *symbol_table_lookup(const char *name);

/**
 * Dump symbol table to console (for debugging)
 */
void symbol_table_dump(void);

#ifdef __cplusplus
}
#endif

#endif /* SYMBOL_TABLE_H */
