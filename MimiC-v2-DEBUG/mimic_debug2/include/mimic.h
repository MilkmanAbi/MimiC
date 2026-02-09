/**
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║  MimiC - Self-Hosted C Compiler & Runtime for RP2040/RP2350               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  VERSION: 1.0.0-alpha                                                     ║
 * ║  ARCHITECTURE: Disk-buffered compilation, kernel-managed execution        ║
 * ║  KERNEL BASE: Stripped Picomimi v14.3.1 resource-owning kernel            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#ifndef MIMIC_H
#define MIMIC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ============================================================================
// PLATFORM DETECTION
// ============================================================================

#ifndef MIMIC_TARGET_RP2350
  #define MIMIC_TARGET_RP2350   0
#endif

#if MIMIC_TARGET_RP2350
  #define MIMIC_TOTAL_RAM       (520 * 1024)
  #define MIMIC_CHIP_NAME       "RP2350"
  #define MIMIC_CORE_COUNT      2
  #define MIMIC_HAS_FPU         1
#else
  #define MIMIC_TOTAL_RAM       (264 * 1024)
  #define MIMIC_CHIP_NAME       "RP2040"
  #define MIMIC_CORE_COUNT      2
  #define MIMIC_HAS_FPU         0
#endif

// ============================================================================
// MEMORY LAYOUT
// ============================================================================

#if MIMIC_TARGET_RP2350
  #define MIMIC_KERNEL_HEAP     (80 * 1024)
  #define MIMIC_USER_HEAP       (380 * 1024)
  #define MIMIC_MAX_TASKS       16
  #define MIMIC_MAX_MEM_BLOCKS  128
#else
  #define MIMIC_KERNEL_HEAP     (50 * 1024)
  #define MIMIC_USER_HEAP       (180 * 1024)
  #define MIMIC_MAX_TASKS       8
  #define MIMIC_MAX_MEM_BLOCKS  64
#endif

#define MIMIC_MEM_ALIGN         32
#define MIMIC_MIN_BLOCK_SPLIT   64
#define MIMIC_KERNEL_RESERVE    (8 * 1024)

// ============================================================================
// .mimi BINARY FORMAT
// ============================================================================

#define MIMI_MAGIC              0x494D494D  // "MIMI"
#define MIMI_VERSION            1

// Architecture codes
#define MIMI_ARCH_CORTEX_M0P    0
#define MIMI_ARCH_CORTEX_M33    1
#define MIMI_ARCH_RISCV         2
#define MIMI_ARCH_THUMB         0   // Alias for Cortex-M0+

// Section IDs
#define MIMI_SECT_NULL          0
#define MIMI_SECT_TEXT          1
#define MIMI_SECT_RODATA        2
#define MIMI_SECT_DATA          3
#define MIMI_SECT_BSS           4

// Binary header - 64 bytes
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t  version;
    uint8_t  flags;
    uint8_t  arch;
    uint8_t  _pad0;
    
    uint32_t entry_offset;
    
    uint32_t text_size;
    uint32_t rodata_size;
    uint32_t data_size;
    uint32_t bss_size;
    
    uint32_t reloc_count;
    uint32_t symbol_count;
    
    uint32_t stack_request;
    uint32_t heap_request;
    
    char     name[16];
    
    uint32_t _reserved[2];
} MimiHeader;

// Relocation types
#define MIMI_RELOC_ABS32        0
#define MIMI_RELOC_REL32        1
#define MIMI_RELOC_THUMB_CALL   2
#define MIMI_RELOC_THUMB_BRANCH 3
#define MIMI_RELOC_DATA_PTR     4

// Relocation entry - 12 bytes
typedef struct __attribute__((packed)) {
    uint32_t offset;
    uint16_t section;
    uint8_t  type;
    uint8_t  _pad;
    uint32_t symbol_idx;
} MimiReloc;

// Symbol types
#define MIMI_SYM_LOCAL          0
#define MIMI_SYM_GLOBAL         1
#define MIMI_SYM_EXTERN         2
#define MIMI_SYM_SYSCALL        3

// Symbol entry - 24 bytes
typedef struct __attribute__((packed)) {
    char     name[16];
    uint32_t value;
    uint8_t  section;
    uint8_t  type;
    uint16_t _pad;
} MimiSymbol;

// ============================================================================
// TASK CONTROL BLOCK
// ============================================================================

typedef enum {
    TASK_STATE_FREE = 0,
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_BLOCKED,
    TASK_STATE_SLEEPING,
    TASK_STATE_ZOMBIE
} MimicTaskState;

typedef struct {
    uintptr_t base;
    uint32_t  total_size;
    
    uint32_t  text_start;
    uint32_t  text_size;
    uint32_t  rodata_start;
    uint32_t  rodata_size;
    uint32_t  data_start;
    uint32_t  data_size;
    uint32_t  bss_start;
    uint32_t  bss_size;
    
    uint32_t  heap_start;
    uint32_t  heap_size;
    uint32_t  heap_used;
    
    uint32_t  stack_top;
    uint32_t  stack_size;
} MimicTaskMem;

typedef struct {
    uint32_t        id;
    char            name[16];
    MimicTaskState  state;
    uint8_t         priority;
    uint8_t         running_core;
    
    void*           entry;
    MimicTaskMem    mem;
    
    uint32_t        wake_time;
    uint32_t        time_slice;
    uint32_t        total_time_us;
    
    uint32_t        start_time;
    uint32_t        alloc_count;
    uint32_t        free_count;
    uint32_t        syscall_count;
    
    uint32_t        sp;
    uint32_t        regs[16];
} MimicTCB;

// ============================================================================
// MEMORY BLOCK TRACKING
// ============================================================================

typedef struct {
    void*       addr;
    uint32_t    size;
    uint32_t    task_id;
    bool        free;
    bool        pinned;
} MimicMemBlock;

// ============================================================================
// SYSCALL NUMBERS
// ============================================================================

#define MIMIC_SYS_EXIT          0
#define MIMIC_SYS_YIELD         1
#define MIMIC_SYS_SLEEP         2
#define MIMIC_SYS_TIME          3

#define MIMIC_SYS_MALLOC        10
#define MIMIC_SYS_FREE          11
#define MIMIC_SYS_REALLOC       12

#define MIMIC_SYS_OPEN          20
#define MIMIC_SYS_CLOSE         21
#define MIMIC_SYS_READ          22
#define MIMIC_SYS_WRITE         23
#define MIMIC_SYS_SEEK          24

#define MIMIC_SYS_PUTCHAR       30
#define MIMIC_SYS_GETCHAR       31
#define MIMIC_SYS_PUTS          32

#define MIMIC_SYS_GPIO_INIT     40
#define MIMIC_SYS_GPIO_DIR      41
#define MIMIC_SYS_GPIO_PUT      42
#define MIMIC_SYS_GPIO_GET      43
#define MIMIC_SYS_GPIO_PULL     44

#define MIMIC_SYS_PWM_INIT      50
#define MIMIC_SYS_PWM_SET_WRAP  51
#define MIMIC_SYS_PWM_SET_LEVEL 52
#define MIMIC_SYS_PWM_ENABLE    53

#define MIMIC_SYS_ADC_INIT      60
#define MIMIC_SYS_ADC_SELECT    61
#define MIMIC_SYS_ADC_READ      62
#define MIMIC_SYS_ADC_TEMP      63

#define MIMIC_SYS_SPI_INIT      70
#define MIMIC_SYS_SPI_WRITE     71
#define MIMIC_SYS_SPI_READ      72
#define MIMIC_SYS_SPI_TRANSFER  73

#define MIMIC_SYS_I2C_INIT      80
#define MIMIC_SYS_I2C_WRITE     81
#define MIMIC_SYS_I2C_READ      82

// ============================================================================
// ERROR CODES
// ============================================================================

#define MIMIC_OK                0
#define MIMIC_ERR_NOMEM         (-1)
#define MIMIC_ERR_INVAL         (-2)
#define MIMIC_ERR_NOENT         (-3)
#define MIMIC_ERR_IO            (-4)
#define MIMIC_ERR_BUSY          (-5)
#define MIMIC_ERR_PERM          (-6)
#define MIMIC_ERR_NOSYS         (-7)
#define MIMIC_ERR_CORRUPT       (-8)
#define MIMIC_ERR_TOOLARGE      (-9)
#define MIMIC_ERR_NOEXEC        (-10)
#define MIMIC_ERR_NOTDIR        (-11)

// ============================================================================
// KERNEL API
// ============================================================================

void mimic_kernel_init(void);
void mimic_kernel_run(void);

void* mimic_kmalloc(size_t size);
void  mimic_kfree(void* ptr);
void* mimic_krealloc(void* ptr, size_t size);

void* mimic_umalloc(uint32_t task_id, size_t size);
void  mimic_ufree(uint32_t task_id, void* ptr);
void  mimic_task_free_all_memory(uint32_t task_id);

int   mimic_task_load(const char* path, uint8_t priority);
int   mimic_task_spawn(const MimiHeader* hdr, const uint8_t* data, uint8_t priority);
void  mimic_task_exit(int code);
void  mimic_task_yield(void);
void  mimic_task_sleep(uint32_t ms);
void  mimic_task_kill(uint32_t task_id);

int   mimic_load_binary(const char* path, MimicTCB* task);
int   mimic_validate_header(const MimiHeader* hdr);

uint32_t mimic_get_free_memory(void);
uint32_t mimic_get_task_count(void);
float    mimic_get_cpu_usage(void);
uint32_t mimic_get_uptime_ms(void);

void mimic_dump_tasks(void);
void mimic_dump_memory(void);

// ============================================================================
// COMPILER CONFIGURATION
// ============================================================================

#define MIMIC_CC_TMP_DIR        "/mimic/tmp"
#define MIMIC_CC_SDK_DIR        "/mimic/sdk"
#define MIMIC_CC_BIN_DIR        "/mimic/bin"
#define MIMIC_CC_SRC_DIR        "/mimic/src"

#define MIMIC_CC_IO_BUFFER      (4 * 1024)
#define MIMIC_CC_MAX_TOKENS     1024
#define MIMIC_CC_MAX_SYMBOLS    512

#define MIMIC_EXT_TOK           ".tok"
#define MIMIC_EXT_AST           ".ast"
#define MIMIC_EXT_IR            ".ir"
#define MIMIC_EXT_OBJ           ".o"
#define MIMIC_EXT_MIMI          ".mimi"

#endif // MIMIC_H
