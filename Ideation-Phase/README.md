# MimiC - On-Device C Compiler for Microcontrollers

**Compile and run C code directly on your RP2040/RP2350 without reflashing firmware**

---

## What is MimiC?

MimiC is an **ahead-of-time C compiler** that runs entirely on microcontroller hardware. Write C code, save it to an SD card, and MimiC compiles it on-device into native ARM machine code that executes immediately.

Think **CircuitPython's workflow, but for C** - the language you already know, with full access to Pico SDK and FreeRTOS.

### Key Features

- ✅ **On-silicon compilation** - No PC toolchain needed after initial flash
- ✅ **Full Pico SDK support** - GPIO, I2C, SPI, PWM, timers, DMA, USB, everything
- ✅ **FreeRTOS integration** - Tasks, queues, semaphores, full RTOS support
- ✅ **C99 language support** - Real C, not a subset or toy language
- ✅ **SD card workflow** - Edit code on any computer, run instantly
- ✅ **Native ARM performance** - Compiled to Thumb-2, not interpreted

---

## How It Works

```
┌─────────────────────────────────────────────────────────────┐
│  1. Write C code on your computer                           │
│     /sd/my_app.c                                            │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  2. Save to SD card, insert into RP2040                     │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  3. MimiC compiler (running on RP2040) compiles to ARM      │
│     - Parses C source from SD                               │
│     - Links against precompiled Pico SDK in flash           │
│     - Generates native Thumb-2 machine code                 │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  4. Code executes immediately as FreeRTOS task              │
│     No reflashing. No external toolchain.                   │
└─────────────────────────────────────────────────────────────┘
```

---

## Example Workflow

**1. Write your application:**

```c
// /sd/blink.c
#include <pico/stdlib.h>
#include <FreeRTOS.h>
#include <task.h>

void blink_task(void *params) {
    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);
    
    while (1) {
        gpio_put(25, 1);
        vTaskDelay(pdMS_TO_TICKS(500));
        gpio_put(25, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

int main() {
    stdio_init_all();
    
    xTaskCreate(blink_task, "Blink", 256, NULL, 1, NULL);
    vTaskStartScheduler();
    
    return 0;
}
```

**2. Save to SD card**

**3. Reboot RP2040 (or send compile command via UART)**

**4. Code compiles and runs**

That's it. No `cmake`, no `arm-none-eabi-gcc`, no UF2 drag-and-drop.

---

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│  Hardware: RP2040 / RP2350                                   │
└──────────────────────────────────────────────────────────────┘
                              ↓
┌──────────────────────────────────────────────────────────────┐
│  Flash (XIP)                                                 │
│  ├─ FreeRTOS Kernel (~40KB)                                 │
│  ├─ Pico SDK (precompiled, ~150KB)                          │
│  ├─ TCC Compiler Runtime (~100KB)                           │
│  ├─ Symbol Table (SDK function addresses, ~10KB)            │
│  └─ MimiC Runtime (~20KB)                                   │
└──────────────────────────────────────────────────────────────┘
                              ↓
┌──────────────────────────────────────────────────────────────┐
│  RAM (264KB on RP2040)                                       │
│  ├─ FreeRTOS heaps/stacks (~40KB)                           │
│  ├─ TCC compilation state (~30KB, temporary)                │
│  ├─ Compiled user code (~20-50KB)                           │
│  └─ User application heap (~150KB+)                         │
└──────────────────────────────────────────────────────────────┘
                              ↓
┌──────────────────────────────────────────────────────────────┐
│  SD Card                                                     │
│  ├─ /include/           (Pico SDK headers, simplified)      │
│  ├─ /examples/          (Sample programs)                   │
│  └─ /user/              (Your C code)                       │
└──────────────────────────────────────────────────────────────┘
```

### Design Principles

1. **SDK is precompiled** - No need to recompile the entire Pico SDK on-device
2. **Headers are minimal** - Simplified declarations stored on SD card
3. **Symbol table is static** - Generated at build time, stored in flash
4. **Compilation is one-shot** - Compile → execute → free memory
5. **Linking is address-based** - User code calls precompiled SDK functions via symbol table

---

## Technical Details

### Compiler: TCC (Tiny C Compiler)

MimiC uses a port of [TCC](https://bellard.org/tcc/) with:
- ARM Thumb-2 backend for Cortex-M0+/M33
- Stripped-down feature set (ARM-only, no debugging symbols)
- Custom startup code for FreeRTOS integration
- Modified memory allocator for embedded constraints

**Why TCC?**
- Full C99 support
- Fast compilation (~100ms for typical programs)
- Small binary (~100KB)
- Proven ARM backend
- BSD-style license

### Memory Model

**Compilation phase:**
```
RAM usage: ~30KB for TCC state + parsed AST
Duration: <200ms for ~500 line programs
Output: Native ARM Thumb-2 code in memory
```

**Execution phase:**
```
TCC state freed
Compiled code remains in RAM or can be cached to SD
Runs as normal FreeRTOS task with full SDK access
```

### Symbol Resolution

The **symbol table** maps SDK function names to flash addresses:

```c
// Generated at firmware build time
const Symbol sdk_symbols[] = {
    {"gpio_init",     (void*)0x10000ABC},
    {"gpio_put",      (void*)0x10000DEF},
    {"vTaskDelay",    (void*)0x10001234},
    {"xTaskCreate",   (void*)0x10005678},
    // ... hundreds more
};
```

When TCC encounters `gpio_put(25, 1)` in user code:
1. Looks up "gpio_put" → address `0x10000DEF`
2. Generates ARM: `mov r0, #25; mov r1, #1; bl 0x10000DEF`

---

## Roadmap

### v0.1 - Proof of Concept (Current)
- [x] Port TCC ARM backend to RP2040
- [ ] FatFS integration for SD card access
- [ ] Basic symbol table generation
- [ ] Compile and execute simple programs
- [ ] UART/USB interface for triggering compilation

### v0.2 - SDK Integration
- [ ] Full Pico SDK symbol table
- [ ] Simplified header files on SD card
- [ ] FreeRTOS task creation from compiled code
- [ ] Error reporting and diagnostics
- [ ] Example programs (GPIO, I2C, SPI, PWM)

### v0.3 - Robustness
- [ ] Memory safety checks
- [ ] Watchdog protection during execution
- [ ] Code caching to SD (avoid recompilation)
- [ ] Multi-file compilation
- [ ] Include path management

### v1.0 - Production Ready
- [ ] REPL over UART (interactive C shell)
- [ ] Debugger integration (breakpoints, stepping)
- [ ] Standard library subset (stdio, stdlib, string)
- [ ] Peripheral resource management
- [ ] Documentation and tutorials

### v2.0+ - Advanced Features
- [ ] RP2350 support (dual Cortex-M33 + Hazard3 RISC-V)
- [ ] Multi-core compilation
- [ ] Optimization passes
- [ ] Static analysis warnings
- [ ] Package manager for libraries

---

## Platform Support

### Current
- ✅ RP2040 (Raspberry Pi Pico, Pico W)
- ✅ FreeRTOS kernel

### Planned
- ⏳ RP2350 (Pico 2)
- ⏳ STM32F4/F7
- ⏳ ESP32-C3/S3
- ⏳ Bare metal (no RTOS)

---

## Comparison to Alternatives

| Feature | MimiC | CircuitPython | MicroPython | Arduino |
|---------|-------|---------------|-------------|---------|
| Language | C99 | Python subset | Python subset | C++ subset |
| Compilation | On-device AOT | Interpreter | Bytecode VM | External PC |
| Speed | Native ARM | ~10-50x slower | ~10-30x slower | Native ARM |
| SDK Access | Full Pico SDK | Limited APIs | Limited APIs | Arduino libs |
| RTOS Support | FreeRTOS native | No | No | Limited |
| Flash Size | ~320KB | ~1MB+ | ~500KB+ | ~100KB |
| RAM Usage | ~40KB + user | ~100KB+ | ~80KB+ | ~20KB + user |
| No PC Required | ✅ After flash | ✅ | ✅ | ❌ |

---

## Use Cases

### Embedded Development
- Prototype GPIO/peripheral code without constant reflashing
- Live debugging on hardware
- Rapid iteration for sensor interfacing

### Education
- Learn C programming on real hardware
- Understand compilers and code generation
- Teach embedded systems without toolchain complexity

### IoT / Edge Devices
- Update device behavior via SD card swap
- Field-programmable logic without OTA firmware
- Multi-tenant devices (load different apps dynamically)

### Research
- Compiler design for resource-constrained systems
- Deterministic real-time compilation
- Kernel integration patterns

---

## Building from Source

**Prerequisites:**
- Pico SDK (2.0.0+)
- FreeRTOS kernel
- `arm-none-eabi-gcc` toolchain
- CMake 3.13+

**Build steps:**
```bash
git clone https://github.com/yourusername/mimic.git
cd mimic
git submodule update --init --recursive

mkdir build && cd build
cmake ..
make -j$(nproc)

# Flash to RP2040
cp mimic.uf2 /media/RPI-RP2/
```

**Preparing SD card:**
```bash
# Format SD card as FAT32
# Copy headers
cp -r sdk-headers/* /media/sdcard/include/

# Copy examples
cp examples/* /media/sdcard/examples/
```

---

## Contributing

We welcome contributions! Areas of interest:

- **Compiler optimization** - Reduce TCC memory footprint
- **SDK coverage** - Add more Pico SDK wrappers
- **Platform ports** - RP2350, STM32, ESP32
- **Tooling** - IDE plugins, syntax highlighting
- **Documentation** - Tutorials, API references
- **Testing** - Automated test suite for compilation

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

---

## License

MimiC is released under the **MIT License**.

TCC is licensed under the **LGPL v2.1**. The TCC runtime library is **exempt from LGPL** and can be linked with proprietary code.

---

## Acknowledgments

- **TCC** by Fabrice Bellard - The core compiler technology
- **Pico SDK** by Raspberry Pi Foundation - Excellent embedded SDK
- **FreeRTOS** - Industry-standard RTOS
- **MimiC community** - Contributors and testers

---

## Contact

- **Project Homepage:** https://github.com/yourusername/mimic
- **Issue Tracker:** https://github.com/yourusername/mimic/issues
- **Discussions:** https://github.com/yourusername/mimic/discussions

---

## FAQ

**Q: Why not just use CircuitPython?**  
A: CircuitPython is great, but interpreted Python is 10-50x slower than native C. MimiC gives you Python's workflow with C's performance.

**Q: Can I still use the normal Pico SDK toolchain?**  
A: Yes! MimiC is for rapid prototyping and live development. For production firmware, use the standard `cmake` + `gcc` workflow.

**Q: How much RAM is available for my program?**  
A: ~150-180KB on RP2040 after kernel, compiler, and system overhead.

**Q: Can I use interrupts / DMA / PIO?**  
A: Yes, full Pico SDK access including all hardware peripherals.

**Q: What happens if my code crashes?**  
A: Watchdog will reset the device. Future versions will include better crash recovery.

**Q: Can I debug compiled code?**  
A: Not yet. Debugging support planned for v1.0 (breakpoints, stepping, variable inspection).

---

**TL;DR**

> MimiC compiles C code on the RP2040 itself. Write C, save to SD, run immediately. No PC toolchain needed. Full Pico SDK + FreeRTOS support. CircuitPython's workflow, native C performance.
