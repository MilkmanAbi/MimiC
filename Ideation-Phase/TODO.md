# MimiC - TODO for v0.1

## Immediate Next Steps (TCC Port)

### Phase 1: Get TCC Source
- [ ] Clone TCC repository
- [ ] Study TCC codebase structure
  - [ ] Identify ARM backend code
  - [ ] Find memory allocator hooks
  - [ ] Understand compilation pipeline
  
### Phase 2: Extract Minimal TCC
- [ ] Create `lib/tcc/` directory structure
- [ ] Copy only ARM-related files:
  - [ ] `tcc.h` - Main header
  - [ ] `libtcc.c` - Core compiler
  - [ ] `arm-gen.c` - ARM code generator
  - [ ] `arm-link.c` - ARM linker
  - [ ] `arm-asm.c` - ARM assembler
- [ ] Remove x86/x64 code
- [ ] Remove debugging code
- [ ] Strip down preprocessor

### Phase 3: Create Embedded Build
- [ ] Create `lib/tcc/CMakeLists.txt`
- [ ] Configure for ARM-only target
- [ ] Set compile flags for RP2040
- [ ] Define `TCC_TARGET_ARM` and `TCC_ARM_EABI`
- [ ] Disable features:
  - [ ] `-DONE_SOURCE` (single compilation unit)
  - [ ] `-DTCC_TARGET_ARM`
  - [ ] `-DCONFIG_TCC_STATIC`
  
### Phase 4: Memory Allocator Integration
- [ ] Find TCC's malloc/free calls
- [ ] Replace with FreeRTOS allocator:
  ```c
  #define tcc_malloc(size) pvPortMalloc(size)
  #define tcc_free(ptr) vPortFree(ptr)
  #define tcc_realloc(ptr, size) custom_realloc(ptr, size)
  ```
- [ ] Implement custom realloc using FreeRTOS heap
- [ ] Add memory tracking for statistics

### Phase 5: File I/O Adaptation
- [ ] TCC expects standard file I/O (`fopen`, `fread`, etc.)
- [ ] Options:
  1. Implement file I/O wrappers around SD card
  2. Use in-memory compilation only (compile strings, not files)
- [ ] **Recommendation:** Start with option 2 (strings only)

### Phase 6: Symbol Table Integration
- [ ] Hook into TCC's `tcc_add_symbol()` function
- [ ] Load all SDK symbols at startup:
  ```c
  for (int i = 0; i < symbol_table_count(); i++) {
      const Symbol *sym = symbol_table_get(i);
      tcc_add_symbol(s, sym->name, sym->address);
  }
  ```

### Phase 7: Test Compilation
- [ ] Write minimal test program:
  ```c
  int add(int a, int b) {
      return a + b;
  }
  ```
- [ ] Compile with TCC
- [ ] Verify generated ARM code
- [ ] Execute and check result

---

## FatFS Integration

### Phase 1: Add FatFS Library
- [ ] Add FatFS as git submodule
- [ ] Create `lib/fatfs/CMakeLists.txt`
- [ ] Build FatFS library

### Phase 2: Implement Disk I/O
- [ ] Implement `disk_initialize()`
- [ ] Implement `disk_read()`
- [ ] Implement `disk_write()`
- [ ] Implement `disk_ioctl()`
- [ ] Implement `get_fattime()`

### Phase 3: SPI Driver
- [ ] Initialize SPI hardware
- [ ] Implement SD card command protocol
- [ ] Handle card initialization sequence
- [ ] Implement read/write blocks

### Phase 4: Test SD Card
- [ ] Format SD card as FAT32
- [ ] Create test file
- [ ] Read test file from RP2040
- [ ] Verify contents

---

## Symbol Table Expansion

### Current Symbols (8 functions)
- gpio_init, gpio_set_dir, gpio_put, gpio_get
- sleep_ms
- stdio_init_all
- vTaskDelay, vTaskStartScheduler

### Add Next (Priority Order)

#### GPIO (High Priority)
- [ ] `gpio_pull_up()`
- [ ] `gpio_pull_down()`
- [ ] `gpio_set_function()`
- [ ] `gpio_set_pulls()`

#### Time (High Priority)
- [ ] `sleep_us()`
- [ ] `get_absolute_time()`
- [ ] `to_ms_since_boot()`

#### Stdio (High Priority)
- [ ] `printf()`
- [ ] `puts()`
- [ ] `getchar()`

#### I2C (Medium Priority)
- [ ] `i2c_init()`
- [ ] `i2c_write_blocking()`
- [ ] `i2c_read_blocking()`

#### SPI (Medium Priority)
- [ ] `spi_init()`
- [ ] `spi_write_blocking()`
- [ ] `spi_read_blocking()`

#### PWM (Medium Priority)
- [ ] `pwm_gpio_to_slice_num()`
- [ ] `pwm_set_wrap()`
- [ ] `pwm_set_chan_level()`
- [ ] `pwm_set_enabled()`

---

## Testing & Validation

### Test Programs Needed

#### Test 1: Basic Compilation
```c
int main() {
    return 42;
}
```

#### Test 2: Function Calls
```c
int add(int a, int b) {
    return a + b;
}

int main() {
    return add(10, 32);
}
```

#### Test 3: Control Flow
```c
int main() {
    int sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += i;
    }
    return sum;
}
```

#### Test 4: SDK Function Call
```c
#include <pico/stdlib.h>

int main() {
    gpio_init(25);
    gpio_set_dir(25, 1);
    gpio_put(25, 1);
    return 0;
}
```

#### Test 5: FreeRTOS Task
```c
#include <FreeRTOS.h>
#include <task.h>

void my_task(void *params) {
    while (1) {
        vTaskDelay(1000);
    }
}

int main() {
    xTaskCreate(my_task, "Test", 256, NULL, 1, NULL);
    vTaskStartScheduler();
    return 0;
}
```

---

## Documentation Tasks

- [ ] Comment all header files
- [ ] Add function documentation
- [ ] Write architecture overview
- [ ] Create memory layout diagram
- [ ] Document compilation process
- [ ] Add troubleshooting guide

---

## Build System

- [ ] Add `pico_sdk_import.cmake`
- [ ] Create proper FreeRTOS integration
- [ ] Set up library dependencies
- [ ] Configure linker script
- [ ] Set memory regions properly

---

## Immediate Blockers

### Critical (Must Fix)
1. **TCC doesn't exist in repo** - Need to add as submodule
2. **FatFS doesn't exist** - Need to add as submodule
3. **FreeRTOS config missing** - Need FreeRTOSConfig.h

### High Priority
4. **No build system** - CMake needs proper configuration
5. **Symbol table incomplete** - Only 8 functions
6. **No actual TCC integration** - All stubs

### Medium Priority
7. **No example programs on SD** - Need real test cases
8. **No headers on SD** - Need simplified SDK headers
9. **No error handling** - Crashes likely

---

## Week-by-Week Plan

### Week 1: TCC Integration
- Days 1-2: Study TCC codebase
- Days 3-4: Extract ARM backend
- Days 5-7: Build and test minimal compilation

### Week 2: SD Card
- Days 1-3: FatFS integration
- Days 4-5: SPI driver
- Days 6-7: File I/O testing

### Week 3: Symbol Table
- Days 1-2: Expand symbol table
- Days 3-4: Create simplified headers
- Days 5-7: Test SDK function calls

### Week 4: Testing & Polish
- Days 1-3: Write test suite
- Days 4-5: Fix bugs
- Days 6-7: Documentation

---

## Help Needed

If you're contributing, these are great starting points:

**Good First Issues:**
- [ ] Add more symbols to symbol_table.c
- [ ] Write example programs
- [ ] Test on real hardware
- [ ] Improve documentation
- [ ] Create simplified SDK headers

**Medium Difficulty:**
- [ ] Implement FatFS disk I/O
- [ ] Port TCC to embedded environment
- [ ] Memory optimization
- [ ] Error message improvements

**Hard Problems:**
- [ ] TCC optimization for code size
- [ ] Debugging support
- [ ] Multi-file compilation
- [ ] Platform ports (STM32, ESP32)
