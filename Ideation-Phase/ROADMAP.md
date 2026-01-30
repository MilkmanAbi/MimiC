# MimiC Development Roadmap

## v0.1 - Proof of Concept (Current Phase)

**Goal:** Demonstrate on-device compilation of simple C programs

### Completed ✓
- [x] Project structure
- [x] FreeRTOS integration
- [x] Basic symbol table
- [x] UART shell interface
- [x] Compilation API design

### In Progress 🔨
- [ ] **TCC Port to RP2040**
  - [ ] Extract TCC ARM backend
  - [ ] Integrate with FreeRTOS memory allocator
  - [ ] Remove x86/x64 codegen (ARM only)
  - [ ] Strip debugging symbol generation
  - [ ] Test basic compilation (int add(int a, int b))

- [ ] **SD Card Integration**
  - [ ] FatFS library integration
  - [ ] SPI driver for SD card
  - [ ] File read/write API
  - [ ] Directory listing

### Planned for v0.1 📋
- [ ] Compile simple programs (functions, expressions, control flow)
- [ ] Link against basic SDK symbols (GPIO, time)
- [ ] Execute compiled code in RAM
- [ ] Basic error reporting

**Target Date:** End of Q1 2026

---

## v0.2 - Full SDK Integration

**Goal:** Support full Pico SDK from compiled code

### Features
- [ ] **Complete Symbol Table**
  - [ ] All GPIO functions
  - [ ] I2C, SPI, UART peripherals
  - [ ] PWM, ADC, timers
  - [ ] DMA, PIO
  - [ ] USB stack
  
- [ ] **Simplified SDK Headers**
  - [ ] Auto-generate from real SDK headers
  - [ ] Type definitions (structs, enums, typedefs)
  - [ ] Constant definitions
  
- [ ] **FreeRTOS API**
  - [ ] Task creation/deletion
  - [ ] Queues, semaphores, mutexes
  - [ ] Timers
  - [ ] Event groups

- [ ] **Multi-file Compilation**
  - [ ] Include path management
  - [ ] Header file parsing
  - [ ] Multiple .c file compilation

**Target Date:** End of Q2 2026

---

## v0.3 - Robustness & Usability

**Goal:** Production-ready compilation system

### Features
- [ ] **Safety & Error Handling**
  - [ ] Watchdog protection during execution
  - [ ] Memory bounds checking
  - [ ] Stack overflow detection
  - [ ] Compilation timeout
  
- [ ] **Code Caching**
  - [ ] Cache compiled binaries to SD
  - [ ] Incremental compilation
  - [ ] Dependency tracking
  
- [ ] **Better Diagnostics**
  - [ ] Line number in error messages
  - [ ] Warning messages
  - [ ] Compilation statistics
  
- [ ] **Standard Library**
  - [ ] stdio subset (printf, sprintf, etc.)
  - [ ] stdlib subset (malloc, free, atoi, etc.)
  - [ ] string functions (memcpy, strcmp, etc.)

**Target Date:** End of Q3 2026

---

## v1.0 - First Stable Release

**Goal:** Feature-complete, stable, documented

### Features
- [ ] **REPL (Read-Eval-Print Loop)**
  - [ ] Interactive C shell
  - [ ] Line editing
  - [ ] Command history
  - [ ] Tab completion
  
- [ ] **Debugging Support**
  - [ ] Breakpoints
  - [ ] Single-stepping
  - [ ] Variable inspection
  - [ ] Call stack traces
  
- [ ] **Documentation**
  - [ ] Complete API reference
  - [ ] Tutorial series
  - [ ] Example programs
  - [ ] Video guides
  
- [ ] **Optimization**
  - [ ] Compilation speed improvements
  - [ ] Code size optimization
  - [ ] Memory usage reduction

**Target Date:** End of Q4 2026

---

## v2.0 - Platform Expansion

**Goal:** Support multiple microcontrollers and RTOSes

### Platforms
- [ ] **RP2350 Support**
  - [ ] Dual Cortex-M33 cores
  - [ ] Hazard3 RISC-V cores
  - [ ] Extended memory (520KB RAM)
  
- [ ] **STM32 Support**
  - [ ] STM32F4 series
  - [ ] STM32F7 series
  - [ ] STM32H7 series
  
- [ ] **ESP32 Support**
  - [ ] ESP32-C3 (RISC-V)
  - [ ] ESP32-S3 (Xtensa)
  
- [ ] **Bare Metal Mode**
  - [ ] No RTOS required
  - [ ] Cooperative scheduling

### Features
- [ ] **Multi-core Compilation**
  - [ ] Compile on one core, run on another
  - [ ] Parallel compilation
  
- [ ] **Advanced Optimization**
  - [ ] Register allocation
  - [ ] Dead code elimination
  - [ ] Constant folding
  
- [ ] **Package Manager**
  - [ ] Library distribution
  - [ ] Dependency resolution
  - [ ] Version management

**Target Date:** 2027

---

## v3.0+ - Future Ideas

### Research & Experimental Features
- [ ] **JIT Compilation**
  - [ ] Compile functions on first call
  - [ ] Adaptive optimization
  
- [ ] **Static Analysis**
  - [ ] Detect common bugs
  - [ ] Security vulnerability scanning
  - [ ] Resource leak detection
  
- [ ] **Higher-Level Languages**
  - [ ] Python-to-C transpiler
  - [ ] Lua integration
  - [ ] Custom DSLs
  
- [ ] **Remote Development**
  - [ ] WiFi/Bluetooth compilation
  - [ ] Web-based IDE
  - [ ] Cloud compilation

---

## Dependency Timeline

```
v0.1 (Q1 2026)
├── TCC port
├── SD card
└── Basic compilation

v0.2 (Q2 2026)
├── Full SDK symbols (depends on v0.1)
├── Multi-file support (depends on v0.1)
└── FreeRTOS API (depends on v0.1)

v0.3 (Q3 2026)
├── Error handling (depends on v0.2)
├── Caching (depends on v0.2)
└── Stdlib (depends on v0.2)

v1.0 (Q4 2026)
├── REPL (depends on v0.3)
├── Debugging (depends on v0.3)
└── Documentation (depends on v0.3)

v2.0 (2027)
├── Multi-platform (depends on v1.0)
└── Optimization (depends on v1.0)
```

---

## Community Goals

### Documentation
- [ ] API reference for all symbols
- [ ] Video tutorials
- [ ] Example project gallery
- [ ] FAQ and troubleshooting guide

### Tooling
- [ ] VS Code extension
- [ ] Syntax highlighting for simplified headers
- [ ] CI/CD for automated testing
- [ ] Benchmark suite

### Community
- [ ] Discord server
- [ ] Monthly development updates
- [ ] Contributor guide
- [ ] Code of conduct

---

## Performance Targets

| Metric | v0.1 | v0.3 | v1.0 |
|--------|------|------|------|
| Compilation Speed | ~5s per 100 LOC | ~2s per 100 LOC | ~1s per 100 LOC |
| Memory Overhead | <40KB | <30KB | <25KB |
| Code Size Overhead | ~320KB | ~250KB | ~200KB |
| Max User Program Size | ~50KB | ~100KB | ~150KB |

---

## Get Involved

Want to contribute? Here's what we need help with:

**High Priority:**
- TCC ARM backend optimization
- FatFS SD card driver
- Symbol table auto-generation
- Documentation writing

**Medium Priority:**
- Example programs
- Testing on real hardware
- Performance benchmarking
- Bug fixing

**Low Priority:**
- Platform ports (STM32, ESP32)
- Advanced features (REPL, debugger)
- Tooling (IDE plugins)

See [CONTRIBUTING.md](CONTRIBUTING.md) for details.
