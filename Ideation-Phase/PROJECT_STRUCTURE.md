# MimiC Project Structure

```
mimic/
├── README.md                    # Main project documentation
├── GETTING_STARTED.md          # Build and usage instructions
├── ROADMAP.md                  # Development timeline and features
├── TODO.md                     # Immediate next steps
├── CMakeLists.txt              # Main build configuration
│
├── src/
│   ├── main.c                  # Entry point and initialization
│   │
│   ├── compiler/
│   │   ├── tcc_port.h         # TCC integration API
│   │   ├── tcc_port.c         # TCC wrapper implementation
│   │   ├── symbol_table.h     # SDK function address mapping
│   │   └── symbol_table.c     # Symbol table implementation
│   │
│   ├── filesystem/
│   │   ├── sd_card.h          # SD card API
│   │   ├── sd_card.c          # FatFS wrapper
│   │   └── fatfs_glue.c       # FatFS hardware integration
│   │
│   ├── runtime/
│   │   ├── executor.h         # Code execution API
│   │   └── executor.c         # Execute compiled programs
│   │
│   └── shell/
│       ├── uart_shell.h       # Command-line interface
│       └── uart_shell.c       # Shell implementation
│
├── examples/
│   ├── blink.c                # Simple LED blink example
│   └── freertos_blink.c       # FreeRTOS task example
│
└── lib/ (to be added)
    ├── FreeRTOS-Kernel/       # FreeRTOS source (git submodule)
    ├── tcc/                   # Tiny C Compiler (git submodule)
    └── fatfs/                 # FatFS library (git submodule)
```

## Component Descriptions

### Core Components

**main.c**
- System initialization
- FreeRTOS task creation
- Heartbeat LED
- Error handlers

**compiler/tcc_port.c**
- TCC compiler integration
- Custom memory allocator
- Symbol table loading
- Compilation API

**compiler/symbol_table.c**
- Maps SDK function names to addresses
- Provides function signatures
- Enables linking user code to precompiled SDK

### Filesystem

**filesystem/sd_card.c**
- Read/write files from SD card
- Directory listing
- File existence checking

**filesystem/fatfs_glue.c**
- SPI hardware initialization
- FatFS disk I/O implementation
- SD card low-level protocol

### Runtime

**runtime/executor.c**
- Executes compiled machine code
- Creates FreeRTOS tasks for user programs
- Manages user code lifecycle

### Shell

**shell/uart_shell.c**
- Interactive command-line interface
- Commands: compile, run, stop, ls, cat, etc.
- File browsing and compilation

## Build Dependencies

```
MimiC
├── Pico SDK (required)
│   ├── Hardware abstraction
│   └── Standard libraries
│
├── FreeRTOS Kernel (required)
│   ├── Task scheduler
│   ├── Memory allocator
│   └── Synchronization primitives
│
├── TCC (required)
│   ├── C compiler frontend
│   ├── ARM code generator
│   └── Linker
│
└── FatFS (required)
    ├── FAT32 filesystem
    └── SD card support
```

## Memory Layout (RP2040)

```
Flash (2MB via XIP)
├── 0x10000000: FreeRTOS Kernel (~40KB)
├── 0x1000A000: Pico SDK Library (~150KB)
├── 0x10030000: TCC Compiler (~100KB)
├── 0x10048000: Symbol Table (~10KB)
└── 0x1004B000: MimiC Runtime (~20KB)

RAM (264KB)
├── 0x20000000: FreeRTOS Stacks (~40KB)
├── 0x2000A000: TCC Work Memory (~30KB, temporary)
├── 0x20012000: Compiled User Code (~50KB)
└── 0x2001E000: User Heap (~144KB)
```

## Compilation Flow

```
1. User writes C code → Saves to SD card
                     ↓
2. Shell receives 'compile' command
                     ↓
3. SD card driver reads source file
                     ↓
4. TCC parses and compiles to ARM code
                     ↓
5. Symbol table provides SDK function addresses
                     ↓
6. Linker patches function calls
                     ↓
7. Generated code stored in RAM
                     ↓
8. Executor runs code as FreeRTOS task
```

## File Conventions

### Source Files
- `.c` files: Implementation
- `.h` files: Public API declarations
- Include guards: `FILENAME_H` format

### Naming
- Functions: `lowercase_with_underscores`
- Types: `PascalCase`
- Constants: `UPPERCASE_WITH_UNDERSCORES`
- Files: `lowercase_with_underscores`

### Comments
- Use Doxygen-style comments for public APIs
- Implementation comments use `//` for single-line
- Block comments use `/* */` for multi-line

## Next Steps

See [TODO.md](TODO.md) for the immediate development tasks, starting with:
1. Adding TCC as a submodule
2. Porting TCC ARM backend to RP2040
3. Implementing FatFS SD card support
4. Testing basic compilation
