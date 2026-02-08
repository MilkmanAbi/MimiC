# MimiC - Self-Hosted C Compiler for RP2040/RP2350

```
╔══════════════════════════════════════════════════════════════╗
║  __  __ _           _  ____                                  ║
║ |  \/  (_)_ __ ___ (_)/ ___|   Self-Hosted C Compiler        ║
║ | |\/| | | '_ ` _ \| | |       for RP2040/RP2350             ║
║ | |  | | | | | | | | | |___                                  ║
║ |_|  |_|_|_| |_| |_|_|\____|   v1.0.0-alpha                  ║
╚══════════════════════════════════════════════════════════════╝
```

## Overview

MimiC is an ambitious project to create a **fully self-hosted C compiler** that runs entirely on RP2040/RP2350 microcontrollers. Write C code on your Pico, compile it on your Pico, and run it on your Pico - no external computer required.

### Key Features

- **Disk-Buffered Compilation**: Uses SD card as working memory for multi-pass compilation
- **Minimal RAM Footprint**: ~4KB per compilation pass
- **Full C89/C90 Support**: Complete C language implementation (planned)
- **Custom Binary Format**: `.mimi` executables with position-independent code
- **Kernel-Managed Execution**: Dynamic loading, memory management, syscalls
- **pico-sdk Compatible API**: Familiar interface for user programs

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     MimiC Architecture                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  source.c ──┬──► [LEXER] ──► source.tok                         │
│             │        ↓                                           │
│             │   [PARSER] ──► source.ast                          │
│             │        ↓                                           │
│             │  [SEMANTIC] ──► source.ir                          │
│             │        ↓                                           │
│             │  [CODEGEN] ──► source.o                            │
│             │        ↓                                           │
│             └──► [LINKER] ──► source.mimi                        │
│                                   ↓                              │
│                           [KERNEL LOADER]                        │
│                                   ↓                              │
│                           ┌─────────────┐                        │
│                           │   RUNNING   │                        │
│                           │   PROCESS   │                        │
│                           └─────────────┘                        │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## Building

### Prerequisites

- Raspberry Pi Pico SDK installed
- CMake 3.13+
- ARM GCC toolchain

### Compile

```bash
# Set up environment
export PICO_SDK_PATH=/path/to/pico-sdk

# Create build directory
mkdir build && cd build

# Configure (RP2040)
cmake ..

# Or for RP2350
cmake -DMIMIC_TARGET_RP2350=ON ..

# Build
make -j4

# Flash
make flash
```

## Usage

Connect via USB serial (115200 baud) and use the built-in shell:

```
mimic> help

Available commands:
  help       Show this help message
  ls         List directory contents
  cat        Display file contents
  cc         Compile C source file
  run        Load and run .mimi binary
  mem        Show memory usage
  tasks      Show running tasks
  info       Show system information
  test       Run compiler tests

mimic> cc /hello.c
Compiling '/hello.c' -> '/hello.mimi'
Compilation successful!
  Tokens:       42
  AST nodes:    28
  IR ops:       15
  Code bytes:   128

mimic> run /hello.mimi
Hello, World!
```

## Project Structure

```
mimic/
├── CMakeLists.txt          # Build configuration
├── README.md               # This file
├── include/
│   ├── mimic.h             # Core types, binary format, kernel API
│   ├── mimic_fat32.h       # FAT32 filesystem and streaming I/O
│   └── mimic_cc.h          # Compiler types and functions
├── src/
│   ├── main.c              # Entry point and shell
│   ├── kernel/
│   │   └── mimic_kernel.c  # Memory management, task loading, syscalls
│   ├── fs/
│   │   └── mimic_fat32.c   # SD card and FAT32 implementation
│   └── compiler/
│       ├── mimic_cc.c      # Compiler infrastructure
│       ├── mimic_lexer.c   # Tokenization (Pass 1)
│       ├── mimic_parser.c  # AST generation (Pass 2)
│       ├── mimic_codegen.c # ARM Thumb code generation (Pass 4)
│       └── mimic_linker.c  # Object linking (Pass 5)
└── sdk/                    # pico-sdk compatible headers (TODO)
```

## .mimi Binary Format

```
┌──────────────────────────────────────────┐
│  Header (64 bytes)                       │
│  ├── magic: "MIMI" (0x494D494D)         │
│  ├── version: 1                          │
│  ├── arch: CORTEX_M0P / CORTEX_M33      │
│  ├── entry_offset                        │
│  ├── section sizes (.text/.rodata/etc)  │
│  ├── relocation count                    │
│  ├── symbol count                        │
│  └── stack/heap requests                 │
├──────────────────────────────────────────┤
│  .text section (executable code)         │
├──────────────────────────────────────────┤
│  .rodata section (constants)             │
├──────────────────────────────────────────┤
│  .data section (initialized globals)     │
├──────────────────────────────────────────┤
│  Relocations (kernel patches at load)    │
├──────────────────────────────────────────┤
│  Symbols (optional, for debugging)       │
└──────────────────────────────────────────┘
```

## Memory Layout

### RP2040 (264KB SRAM)
- Kernel heap: 50KB
- User heap: 180KB
- Max tasks: 8

### RP2350 (520KB SRAM)
- Kernel heap: 80KB
- User heap: 380KB
- Max tasks: 16

## Status

### What's Working ✅
- Lexer: Full C89 tokenization
- Parser: Expressions, statements, function declarations
- FAT32: Complete SD card I/O with streaming functions
- Kernel: Memory management, task loading with relocation

### What's In Progress 🚧
- Semantic analysis pass
- ARM Thumb code generator
- Preprocessor (#include, #define)

### What's Planned 📋
- Complete C89 features (structs, unions, enums, switch)
- Standard library (printf, malloc, string functions)
- Debugger support
- RISC-V backend for RP2350 Hazard3 core

## License

MIT License - See LICENSE file for details

## Acknowledgments

- Kernel base derived from Picomimi v14.3.1
- Inspired by Fabrice Bellard's TinyCC
- Thanks to the Raspberry Pi Foundation for the RP2040/RP2350

---

*"Any sufficiently advanced microcontroller is indistinguishable from a tiny computer."*
