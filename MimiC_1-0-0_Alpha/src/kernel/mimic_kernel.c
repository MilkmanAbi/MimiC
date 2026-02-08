/**
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║  MimiC Kernel - Core Implementation                                       ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  Memory management + Task loading + Syscall dispatch                      ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/mutex.h"
#include "pico/critical_section.h"
#include "hardware/timer.h"
#include "hardware/sync.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/watchdog.h"

#include <string.h>
#include <stdio.h>

#include "mimic.h"
#include "mimic_fat32.h"

// ============================================================================
// COMPILER HINTS
// ============================================================================

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#define HOT_FUNC        __attribute__((hot, optimize("O3")))
#define ALIGNED(n)      __attribute__((aligned(n)))

// ============================================================================
// STATIC MEMORY POOLS
// ============================================================================

static uint8_t ALIGNED(32) kernel_heap[MIMIC_KERNEL_HEAP];
static uint8_t ALIGNED(32) user_heap[MIMIC_USER_HEAP];

// ============================================================================
// KERNEL STATE
// ============================================================================

typedef struct {
    MimicMemBlock   mem_blocks[MIMIC_MAX_MEM_BLOCKS];
    uint32_t        mem_block_count;
    mutex_t         mem_lock;
    
    MimicMemBlock   user_blocks[MIMIC_MAX_MEM_BLOCKS];
    uint32_t        user_block_count;
    mutex_t         user_lock;
    
    MimicTCB        tasks[MIMIC_MAX_TASKS];
    uint8_t         task_count;
    uint8_t         current_task;
    mutex_t         task_lock;
    critical_section_t sched_cs;
    
    uint64_t        tick_count;
    uint64_t        last_schedule_us;
    bool            running;
    bool            preempt_pending;
    
    uint64_t        boot_time_us;
    
    uint32_t        total_allocs;
    uint32_t        total_frees;
    uint32_t        failed_allocs;
    uint32_t        programs_loaded;
    uint32_t        syscalls_handled;
    uint32_t        context_switches;
    
    uint32_t        kernel_free;
    uint32_t        user_free;
    
    bool            fs_mounted;
} MimicKernel;

static MimicKernel kernel;

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

static void mem_init(void) {
    mutex_init(&kernel.mem_lock);
    mutex_init(&kernel.user_lock);
    
    kernel.mem_blocks[0].addr = kernel_heap;
    kernel.mem_blocks[0].size = MIMIC_KERNEL_HEAP;
    kernel.mem_blocks[0].task_id = 0;
    kernel.mem_blocks[0].free = true;
    kernel.mem_blocks[0].pinned = false;
    kernel.mem_block_count = 1;
    kernel.kernel_free = MIMIC_KERNEL_HEAP;
    
    kernel.user_blocks[0].addr = user_heap;
    kernel.user_blocks[0].size = MIMIC_USER_HEAP;
    kernel.user_blocks[0].task_id = 0;
    kernel.user_blocks[0].free = true;
    kernel.user_blocks[0].pinned = false;
    kernel.user_block_count = 1;
    kernel.user_free = MIMIC_USER_HEAP;
}

static void* mem_alloc_from_pool(MimicMemBlock* blocks, uint32_t* block_count,
                                  uint32_t* free_bytes, mutex_t* lock,
                                  size_t size, uint32_t task_id, uint32_t max_blocks) {
    if (size == 0) return NULL;
    
    size = (size + MIMIC_MEM_ALIGN - 1) & ~(MIMIC_MEM_ALIGN - 1);
    
    mutex_enter_blocking(lock);
    
    void* result = NULL;
    uint32_t best_idx = 0xFFFFFFFF;
    uint32_t best_size = 0xFFFFFFFF;
    
    for (uint32_t i = 0; i < *block_count; i++) {
        if (blocks[i].free && blocks[i].size >= size) {
            if (blocks[i].size < best_size) {
                best_idx = i;
                best_size = blocks[i].size;
                if (best_size == size) break;
            }
        }
    }
    
    if (best_idx != 0xFFFFFFFF) {
        MimicMemBlock* block = &blocks[best_idx];
        
        if (block->size > size + MIMIC_MIN_BLOCK_SPLIT && 
            *block_count < max_blocks) {
            MimicMemBlock* new_block = &blocks[(*block_count)++];
            new_block->addr = (uint8_t*)block->addr + size;
            new_block->size = block->size - size;
            new_block->task_id = 0;
            new_block->free = true;
            new_block->pinned = false;
            
            block->size = size;
        }
        
        block->free = false;
        block->task_id = task_id;
        *free_bytes -= block->size;
        kernel.total_allocs++;
        
        result = block->addr;
    } else {
        kernel.failed_allocs++;
    }
    
    mutex_exit(lock);
    return result;
}

static void mem_free_in_pool(MimicMemBlock* blocks, uint32_t* block_count,
                              uint32_t* free_bytes, mutex_t* lock, void* ptr) {
    if (!ptr) return;
    
    mutex_enter_blocking(lock);
    
    for (uint32_t i = 0; i < *block_count; i++) {
        if (blocks[i].addr == ptr && !blocks[i].free) {
            if (blocks[i].pinned) {
                mutex_exit(lock);
                return;
            }
            
            blocks[i].free = true;
            *free_bytes += blocks[i].size;
            kernel.total_frees++;
            break;
        }
    }
    
    mutex_exit(lock);
}

void* HOT_FUNC mimic_kmalloc(size_t size) {
    return mem_alloc_from_pool(kernel.mem_blocks, &kernel.mem_block_count,
                                &kernel.kernel_free, &kernel.mem_lock,
                                size, 0, MIMIC_MAX_MEM_BLOCKS);
}

void HOT_FUNC mimic_kfree(void* ptr) {
    mem_free_in_pool(kernel.mem_blocks, &kernel.mem_block_count,
                      &kernel.kernel_free, &kernel.mem_lock, ptr);
}

void* mimic_krealloc(void* ptr, size_t size) {
    if (!ptr) return mimic_kmalloc(size);
    if (size == 0) {
        mimic_kfree(ptr);
        return NULL;
    }
    
    void* new_ptr = mimic_kmalloc(size);
    if (new_ptr) {
        // Find old size
        mutex_enter_blocking(&kernel.mem_lock);
        uint32_t old_size = 0;
        for (uint32_t i = 0; i < kernel.mem_block_count; i++) {
            if (kernel.mem_blocks[i].addr == ptr) {
                old_size = kernel.mem_blocks[i].size;
                break;
            }
        }
        mutex_exit(&kernel.mem_lock);
        
        if (old_size > 0) {
            memcpy(new_ptr, ptr, old_size < size ? old_size : size);
        }
        mimic_kfree(ptr);
    }
    return new_ptr;
}

void* mimic_umalloc(uint32_t task_id, size_t size) {
    return mem_alloc_from_pool(kernel.user_blocks, &kernel.user_block_count,
                                &kernel.user_free, &kernel.user_lock,
                                size, task_id, MIMIC_MAX_MEM_BLOCKS);
}

void mimic_ufree(uint32_t task_id, void* ptr) {
    (void)task_id;  // TODO: verify ownership
    mem_free_in_pool(kernel.user_blocks, &kernel.user_block_count,
                      &kernel.user_free, &kernel.user_lock, ptr);
}

void mimic_task_free_all_memory(uint32_t task_id) {
    mutex_enter_blocking(&kernel.user_lock);
    
    for (uint32_t i = 0; i < kernel.user_block_count; i++) {
        if (kernel.user_blocks[i].task_id == task_id && !kernel.user_blocks[i].free) {
            kernel.user_blocks[i].free = true;
            kernel.user_free += kernel.user_blocks[i].size;
            kernel.total_frees++;
        }
    }
    
    mutex_exit(&kernel.user_lock);
}

// ============================================================================
// TASK MANAGEMENT
// ============================================================================

static void task_init(void) {
    mutex_init(&kernel.task_lock);
    critical_section_init(&kernel.sched_cs);
    
    // Task 0 is the kernel/idle task
    memset(&kernel.tasks[0], 0, sizeof(MimicTCB));
    kernel.tasks[0].id = 0;
    strcpy(kernel.tasks[0].name, "kernel");
    kernel.tasks[0].state = TASK_STATE_RUNNING;
    kernel.tasks[0].priority = 255;  // Lowest
    
    kernel.task_count = 1;
    kernel.current_task = 0;
}

static MimicTCB* task_alloc(void) {
    mutex_enter_blocking(&kernel.task_lock);
    
    MimicTCB* task = NULL;
    for (uint8_t i = 1; i < MIMIC_MAX_TASKS; i++) {
        if (kernel.tasks[i].state == TASK_STATE_FREE) {
            task = &kernel.tasks[i];
            memset(task, 0, sizeof(MimicTCB));
            task->id = i;
            task->state = TASK_STATE_READY;
            kernel.task_count++;
            break;
        }
    }
    
    mutex_exit(&kernel.task_lock);
    return task;
}

int mimic_validate_header(const MimiHeader* hdr) {
    if (hdr->magic != MIMI_MAGIC) return MIMIC_ERR_NOEXEC;
    if (hdr->version != MIMI_VERSION) return MIMIC_ERR_NOEXEC;
    
#if MIMIC_TARGET_RP2350
    if (hdr->arch != MIMI_ARCH_CORTEX_M33 && hdr->arch != MIMI_ARCH_RISCV) {
        return MIMIC_ERR_NOEXEC;
    }
#else
    if (hdr->arch != MIMI_ARCH_CORTEX_M0P) {
        return MIMIC_ERR_NOEXEC;
    }
#endif
    
    return MIMIC_OK;
}

int mimic_load_binary(const char* path, MimicTCB* task) {
    int fd = mimic_fopen(path, MIMIC_FILE_READ);
    if (fd < 0) return fd;
    
    // Read header
    MimiHeader hdr;
    int n = mimic_fread(fd, &hdr, sizeof(hdr));
    if (n != sizeof(hdr)) {
        mimic_fclose(fd);
        return MIMIC_ERR_CORRUPT;
    }
    
    int err = mimic_validate_header(&hdr);
    if (err != MIMIC_OK) {
        mimic_fclose(fd);
        return err;
    }
    
    // Calculate total memory needed
    uint32_t code_size = hdr.text_size + hdr.rodata_size;
    uint32_t data_size = hdr.data_size + hdr.bss_size;
    uint32_t stack_size = hdr.stack_request ? hdr.stack_request : 4096;
    uint32_t heap_size = hdr.heap_request ? hdr.heap_request : 8192;
    uint32_t total_size = code_size + data_size + stack_size + heap_size;
    
    total_size = (total_size + 31) & ~31;  // Align
    
    // Allocate memory
    void* mem = mimic_umalloc(task->id, total_size);
    if (!mem) {
        mimic_fclose(fd);
        return MIMIC_ERR_NOMEM;
    }
    
    // Setup memory layout
    task->mem.base = (uintptr_t)mem;
    task->mem.total_size = total_size;
    
    task->mem.text_start = 0;
    task->mem.text_size = hdr.text_size;
    
    task->mem.rodata_start = hdr.text_size;
    task->mem.rodata_size = hdr.rodata_size;
    
    task->mem.data_start = code_size;
    task->mem.data_size = hdr.data_size;
    
    task->mem.bss_start = code_size + hdr.data_size;
    task->mem.bss_size = hdr.bss_size;
    
    task->mem.heap_start = code_size + data_size;
    task->mem.heap_size = heap_size;
    task->mem.heap_used = 0;
    
    task->mem.stack_top = total_size;
    task->mem.stack_size = stack_size;
    
    // Load sections
    uint8_t* base = (uint8_t*)mem;
    
    // Load .text
    if (hdr.text_size > 0) {
        n = mimic_fread(fd, base + task->mem.text_start, hdr.text_size);
        if (n != (int)hdr.text_size) {
            mimic_ufree(task->id, mem);
            mimic_fclose(fd);
            return MIMIC_ERR_CORRUPT;
        }
    }
    
    // Load .rodata
    if (hdr.rodata_size > 0) {
        n = mimic_fread(fd, base + task->mem.rodata_start, hdr.rodata_size);
        if (n != (int)hdr.rodata_size) {
            mimic_ufree(task->id, mem);
            mimic_fclose(fd);
            return MIMIC_ERR_CORRUPT;
        }
    }
    
    // Load .data
    if (hdr.data_size > 0) {
        n = mimic_fread(fd, base + task->mem.data_start, hdr.data_size);
        if (n != (int)hdr.data_size) {
            mimic_ufree(task->id, mem);
            mimic_fclose(fd);
            return MIMIC_ERR_CORRUPT;
        }
    }
    
    // Zero .bss
    if (hdr.bss_size > 0) {
        memset(base + task->mem.bss_start, 0, hdr.bss_size);
    }
    
    // Process relocations
    for (uint32_t i = 0; i < hdr.reloc_count; i++) {
        MimiReloc reloc;
        n = mimic_fread(fd, &reloc, sizeof(reloc));
        if (n != sizeof(reloc)) break;
        
        uint32_t* target = NULL;
        switch (reloc.section) {
            case MIMI_SECT_TEXT:
                target = (uint32_t*)(base + task->mem.text_start + reloc.offset);
                break;
            case MIMI_SECT_RODATA:
                target = (uint32_t*)(base + task->mem.rodata_start + reloc.offset);
                break;
            case MIMI_SECT_DATA:
                target = (uint32_t*)(base + task->mem.data_start + reloc.offset);
                break;
        }
        
        if (target) {
            // Add base address to relocation
            *target += (uint32_t)task->mem.base;
        }
    }
    
    mimic_fclose(fd);
    
    // Set entry point
    task->entry = (void*)(task->mem.base + task->mem.text_start + hdr.entry_offset);
    
    // Copy name
    strncpy(task->name, hdr.name, 15);
    task->name[15] = '\0';
    
    // Initialize stack pointer
    task->sp = task->mem.base + task->mem.stack_top;
    
    kernel.programs_loaded++;
    
    return MIMIC_OK;
}

int mimic_task_load(const char* path, uint8_t priority) {
    MimicTCB* task = task_alloc();
    if (!task) return MIMIC_ERR_NOMEM;
    
    int err = mimic_load_binary(path, task);
    if (err != MIMIC_OK) {
        task->state = TASK_STATE_FREE;
        kernel.task_count--;
        return err;
    }
    
    task->priority = priority;
    task->start_time = time_us_64() / 1000;
    task->state = TASK_STATE_READY;
    
    return (int)task->id;
}

void mimic_task_kill(uint32_t task_id) {
    if (task_id == 0 || task_id >= MIMIC_MAX_TASKS) return;
    
    MimicTCB* task = &kernel.tasks[task_id];
    if (task->state == TASK_STATE_FREE) return;
    
    // Free all task memory
    mimic_task_free_all_memory(task_id);
    
    // Mark as free
    task->state = TASK_STATE_FREE;
    kernel.task_count--;
}

// ============================================================================
// SCHEDULER
// ============================================================================

static void scheduler_tick(void) {
    if (!kernel.running) return;
    
    uint64_t now_us = time_us_64();
    kernel.tick_count++;
    
    critical_section_enter_blocking(&kernel.sched_cs);
    
    uint64_t now_ms = now_us / 1000;
    for (uint8_t i = 1; i < MIMIC_MAX_TASKS; i++) {
        if (kernel.tasks[i].state == TASK_STATE_SLEEPING) {
            if (now_ms >= kernel.tasks[i].wake_time) {
                kernel.tasks[i].state = TASK_STATE_READY;
            }
        }
    }
    
    MimicTCB* next = &kernel.tasks[0];
    uint8_t best_prio = 255;
    
    for (uint8_t i = 1; i < MIMIC_MAX_TASKS; i++) {
        if (kernel.tasks[i].state == TASK_STATE_READY) {
            if (kernel.tasks[i].priority < best_prio) {
                best_prio = kernel.tasks[i].priority;
                next = &kernel.tasks[i];
            }
        }
    }
    
    if (next->id != kernel.current_task) {
        kernel.tasks[kernel.current_task].state = TASK_STATE_READY;
        kernel.current_task = next->id;
        next->state = TASK_STATE_RUNNING;
        kernel.context_switches++;
    }
    
    kernel.last_schedule_us = now_us;
    
    critical_section_exit(&kernel.sched_cs);
}

// ============================================================================
// SYSCALL HANDLERS
// ============================================================================

int32_t mimic_syscall(uint32_t num, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)a3;
    kernel.syscalls_handled++;
    uint32_t task_id = kernel.current_task;
    
    switch (num) {
        case MIMIC_SYS_EXIT:
            mimic_task_kill(task_id);
            return 0;
            
        case MIMIC_SYS_YIELD:
            mimic_task_yield();
            return 0;
            
        case MIMIC_SYS_SLEEP:
            mimic_task_sleep(a0);
            return 0;
            
        case MIMIC_SYS_TIME:
            return (time_us_64() - kernel.boot_time_us) / 1000;
            
        case MIMIC_SYS_MALLOC:
            return (uint32_t)mimic_umalloc(task_id, a0);
            
        case MIMIC_SYS_FREE:
            mimic_ufree(task_id, (void*)a0);
            return 0;
            
        case MIMIC_SYS_PUTCHAR:
            putchar(a0);
            return a0;
            
        case MIMIC_SYS_GETCHAR:
            return getchar();
            
        case MIMIC_SYS_PUTS:
            printf("%s", (const char*)a0);
            return 0;
            
        case MIMIC_SYS_GPIO_INIT:
            gpio_init(a0);
            return 0;
            
        case MIMIC_SYS_GPIO_DIR:
            gpio_set_dir(a0, a1);
            return 0;
            
        case MIMIC_SYS_GPIO_PUT:
            gpio_put(a0, a1);
            return 0;
            
        case MIMIC_SYS_GPIO_GET:
            return gpio_get(a0);
            
        case MIMIC_SYS_GPIO_PULL:
            if (a1 == 1) gpio_pull_up(a0);
            else if (a1 == 2) gpio_pull_down(a0);
            else gpio_disable_pulls(a0);
            return 0;
            
        case MIMIC_SYS_OPEN:
            return mimic_fopen((const char*)a0, a1);
            
        case MIMIC_SYS_CLOSE:
            return mimic_fclose(a0);
            
        case MIMIC_SYS_READ:
            return mimic_fread(a0, (void*)a1, a2);
            
        case MIMIC_SYS_WRITE:
            return mimic_fwrite(a0, (const void*)a1, a2);
            
        case MIMIC_SYS_SEEK:
            return mimic_fseek(a0, a1, a2);
            
        default:
            return MIMIC_ERR_NOSYS;
    }
}

void mimic_task_yield(void) {
    kernel.preempt_pending = true;
    scheduler_tick();
}

void mimic_task_sleep(uint32_t ms) {
    if (kernel.current_task == 0) return;
    
    MimicTCB* task = &kernel.tasks[kernel.current_task];
    task->wake_time = (time_us_64() / 1000) + ms;
    task->state = TASK_STATE_SLEEPING;
    
    scheduler_tick();
}

void mimic_task_exit(int code) {
    (void)code;
    mimic_task_kill(kernel.current_task);
    scheduler_tick();
}

// ============================================================================
// PUBLIC API
// ============================================================================

uint32_t mimic_get_free_memory(void) {
    return kernel.user_free;
}

uint32_t mimic_get_task_count(void) {
    return kernel.task_count;
}

uint32_t mimic_get_uptime_ms(void) {
    return (time_us_64() - kernel.boot_time_us) / 1000;
}

float mimic_get_cpu_usage(void) {
    return 0.0f;  // TODO
}

void mimic_dump_tasks(void) {
    printf("\n=== MIMIC TASKS ===\n");
    printf("ID  NAME            STATE    PRI  MEM\n");
    for (uint8_t i = 0; i < MIMIC_MAX_TASKS; i++) {
        MimicTCB* t = &kernel.tasks[i];
        if (t->state != TASK_STATE_FREE) {
            const char* state_str[] = {"FREE", "READY", "RUN", "BLOCK", "SLEEP", "ZOMB"};
            printf("%2lu %-15s %-7s  %3d  %lu\n",
                   (unsigned long)t->id, t->name, state_str[t->state], 
                   t->priority, (unsigned long)t->mem.total_size);
        }
    }
}

void mimic_dump_memory(void) {
    printf("\n=== MIMIC MEMORY ===\n");
    printf("Kernel: %lu / %d bytes free\n", (unsigned long)kernel.kernel_free, MIMIC_KERNEL_HEAP);
    printf("User:   %lu / %d bytes free\n", (unsigned long)kernel.user_free, MIMIC_USER_HEAP);
    printf("Allocs: %lu  Frees: %lu  Failed: %lu\n",
           (unsigned long)kernel.total_allocs, 
           (unsigned long)kernel.total_frees, 
           (unsigned long)kernel.failed_allocs);
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void mimic_kernel_init(void) {
    kernel.boot_time_us = time_us_64();
    kernel.running = false;
    
    mem_init();
    task_init();
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════╗\n");
    printf("║  MimiC Kernel v1.0.0 - " MIMIC_CHIP_NAME "                  ║\n");
    printf("╚═══════════════════════════════════════════════╝\n");
    printf("Kernel heap: %d KB\n", MIMIC_KERNEL_HEAP / 1024);
    printf("User heap:   %d KB\n", MIMIC_USER_HEAP / 1024);
    printf("Max tasks:   %d\n\n", MIMIC_MAX_TASKS);
}

void mimic_kernel_run(void) {
    int err = mimic_fat32_mount();
    if (err == MIMIC_OK) {
        kernel.fs_mounted = true;
        printf("[FS] SD card mounted\n");
    } else {
        printf("[FS] Mount failed: %d\n", err);
    }
    
    kernel.running = true;
    
    printf("[KERNEL] Running...\n\n");
    
    while (kernel.running) {
        scheduler_tick();
        
        MimicTCB* current = &kernel.tasks[kernel.current_task];
        if (current->id != 0 && current->state == TASK_STATE_RUNNING) {
            // Execute task - in real implementation would do context switch
        }
        
        if (kernel.current_task == 0) {
            __wfi();
        }
    }
}
