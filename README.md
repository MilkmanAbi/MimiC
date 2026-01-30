# MimiC

**On‑silicon ahead‑of‑time C compiler for microcontrollers**
*(CircuitPython‑style workflow, native C performance)*
(＾▽＾)

---

## What is MimiC?

**MimiC** is an **on-device, ahead-of-time (AOT) C compiler** for resource-constrained microcontrollers like the **RP2040 / RP2350**.

Instead of reflashing firmware for every change, you can:

1. Write standard **C99** code
2. Save it to an SD card
3. Insert it into the device
4. Compile and run **on the MCU itself**, instantly

> MimiC also **saves compiled binaries back to the SD card**, so future runs can skip compilation entirely — no AOT delay required.

No external toolchain. No interpreter. No reflashing loop.

Think *CircuitPython workflow*, but with **real C**, compiled to **native ARM machine code**.

---

## Key Features

* Full **C99** support (not a toy language)
* **On-silicon compilation** (no PC needed)
* Native **ARM Thumb** machine code output
* Direct access to **Pico SDK**: GPIO, I2C, SPI, PWM, DMA, USB, timers
* **FreeRTOS-native** execution model
* **SD card workflow**: source → compile → cache binary → future reload
* Deterministic, fast compilation
* Designed for **runtime-loaded applications**

(￣ー￣)ｂ

---

## Why MimiC Exists

Typical embedded workflows force a trade-off:

* **C/C++** → fast, full control, but reflashing is slow
* **Python-like systems** → rapid iteration, but slow and limited

MimiC removes this compromise:

* Native performance
* Full SDK access
* Rapid iteration **on hardware**
* Persistent SD-card caching for instant reload

No VM. No slow interpreter. Just **real C running dynamically**.

---

## How It Works

```
┌─────────────────────────────────────────────┐
│ Write C source code                         │
│ /sd/user/blink.c                            │
└─────────────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────┐
│ Insert SD card / trigger compile            │
└─────────────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────┐
│ MimiC compiler runs on MCU                  │
│ • Parses C source                           │
│ • Resolves SDK symbols                      │
│ • Generates native ARM code                 │
│ • Saves compiled binary to SD for future    │
│   instant load                              │
└─────────────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────┐
│ Code executes as FreeRTOS task              │
│ No reflashing, no reboot loop               │
└─────────────────────────────────────────────┘
```

Compilation happens **once per change**, then the temporary compiler state is freed. Reloading from SD card skips compilation entirely.

---

## Example

```c
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

int main(void) {
    stdio_init_all();
    xTaskCreate(blink_task, "blink", 256, NULL, 1, NULL);
    vTaskStartScheduler();
    return 0;
}
```

Save to SD card, compile, and run. The compiled binary is cached for instant execution next time.

(｀・ω・´)

---

## Architecture Overview

### Flash (XIP)

* FreeRTOS kernel
* Precompiled Pico SDK
* TCC compiler runtime
* Static SDK symbol table
* MimiC control runtime

### RAM

* FreeRTOS stacks and heaps
* Temporary compiler state (freed after compile)
* Compiled user code
* User application heap

### SD Card

* `/user/` — application source files
* `/include/` — simplified SDK headers
* `/examples/` — sample programs
* `/bin/` — cached compiled binaries for instant reload

---

## Compiler Choice

**MimiC uses Tiny C Compiler (TCC).**

**Why TCC?**

* Full C99 support
* Extremely fast compilation
* Small footprint
* Simple, hackable codebase
* Proven ARM backend

MimiC focuses on **running a real compiler on a microcontroller** — not reinventing C.

(￣︶￣)

---

## Symbol Resolution Model

The Pico SDK is **precompiled** and stored in flash.

MimiC builds a static symbol table:

```c
{"gpio_put",    (void*)0x10000DEF}
{"vTaskDelay", (void*)0x10001234}
```

User code calls these addresses directly — **no dynamic linker**, **no relocation**.
Compilation is fast and memory usage is predictable.

---

## Design Principles

1. SDK code is immutable
2. User code is replaceable at runtime
3. Compilation is **one-shot**
4. No background JIT or interpreter
5. Failure modes are explicit

MimiC is **infrastructure**, not a scripting toy.

---

## Comparison

| System        | Language | Execution   | Speed  | Toolchain Required |
| ------------- | -------- | ----------- | ------ | ------------------ |
| MimiC         | C99      | AOT native  | Fast   | No (after flash)   |
| CircuitPython | Python   | Interpreter | Slow   | No                 |
| MicroPython   | Python   | Bytecode VM | Medium | No                 |
| Arduino       | C++      | Native      | Fast   | Yes                |

---

## Use Cases

* Rapid embedded prototyping
* Education and systems learning
* Field-reconfigurable devices
* Research on embedded compilation
* MicroOS-style runtime application loading

---

## License

* MimiC: MIT License
* TCC: LGPL v2.1 (runtime exempt)

---

## Status

MimiC is experimental and unapologetically low-level.

If you like **compilers, RTOSes, and running code where it shouldn’t fit — welcome**.

(⌐■_■)

I have no idea what the fuck I am doing, rahhhh!
---
