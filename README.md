# MimiC

**Deterministic, embeddable systems scripting language for microcontrollers**

---

## Overview

MimiC is a **C-inspired, deterministic scripting language** designed to run on microcontrollers and embedded kernels like **Picomimi**, as well as RTOS environments such as FreeRTOS. It is small, portable, and hardware-aware, enabling live scripting on real hardware without unsafe code injection.

Unlike traditional scripting languages that assume full operating systems or flat memory models, MimiC **respects resource ownership, deterministic memory, and kernel-managed execution**, making it ideal for:

* microcontrollers with tight RAM and flash constraints
* real-time capable kernels
* safe execution of scripts without compromising system stability

---

## Project Goals

MimiC is designed with the following goals in mind:

1. **Deterministic execution**

   * Avoid undefined behavior and unsafe memory access
   * Scripts cannot bypass kernel-managed resources

2. **Kernel integration**

   * Designed to work with kernels like Picomimi or FreeRTOS
   * Exposes kernel services safely through a **host ABI**

3. **Portability**

   * Small, minimal runtime allows deployment across multiple MCU families
   * Minimal dependencies beyond a C99-capable compiler

4. **Scripting as first-class citizen**

   * Scripts run live on hardware, enabling dynamic applications
   * Allows building higher-level languages (Python, shell, DSLs) on top

5. **Safety & resource ownership**

   * Pins, timers, peripherals, and memory are fully managed
   * Scripts cannot accidentally take over hardware or memory

6. **Extensible standard library**

   * Start with kernel-oriented primitives
   * Expand to higher-level modules for tasks, I/O, and system integration

---

## Design Philosophy

MimiC’s philosophy is **“Big surface, small core”**:

* The **core** is minimal: interpreter, deterministic memory, scheduling hooks, kernel ABI
* The **surface** can be expanded: libraries, modules, shell integration, and higher-level scripting languages
* Safety is enforced at the **core level**, letting users innovate without breaking the system

Unlike full-blown embedded Python or CircuitPython, MimiC is **not a Python clone**. It is a **C-shaped systems language**, designed for:

* **embedded scripting**
* **hardware-aware automation**
* **deterministic control**

---

## Features (Planned / v0.1)

* **C-inspired syntax**
* **Expressions and control flow** (`if`, `while`, `for`)
* **Functions with local variables**
* **Kernel intrinsics** for resource-safe hardware access
* **Deterministic memory model** (GC or region-based)
* **Cooperative task support** (scheduler integration)
* **Script loading / execution** at runtime
* **Error handling with precise reporting**

Future expansion may include:

* Structs and arrays with bounds checking
* Modules and import system
* Higher-level scripting languages on top (Python, shell, DSLs)
* REPL / live debugging
* Optional unsafe modes for low-level hardware control

---

## Architecture

```
Hardware (RP2040, STM32, etc.)
        ↑
Picomimi Kernel / RTOS
        ↑
MimiC Runtime / VM
        ↑
MimiC Language
        ↑
Shell / Higher-level languages (Python, DSLs)
```

* **Hardware**: the physical MCU peripherals
* **Kernel / RTOS**: memory, scheduler, interrupts, peripheral ownership
* **MimiC Runtime**: interpreter or bytecode VM, deterministic execution, GC hooks
* **MimiC Language**: C-inspired syntax for safe scripting
* **Shell / High-level languages**: optional scripting layers built on top

---

## Why MimiC Exists

Running scripts directly on hardware is **dangerous and error-prone**. Traditional embedded scripting either:

* exposes unsafe memory and hardware, or
* is too heavy to run on small MCUs

MimiC solves this by providing a **safe, deterministic, embeddable language** with **first-class kernel integration**, enabling:

* live script execution
* hardware automation# MimiC
MimiC is a deterministic, embeddable systems-scripting language for microcontrollers. Designed for kernels like Picomimi and RTOS environments, it provides safe memory models, resource ownership, and real-time friendly execution while staying small, portable, and hardware-aware.

* higher-level language layers without kernel hacks

It’s a modern **replacement for DIY scripting languages**, optimized for small embedded systems.

---

## Potential Use Cases

* Live scripting on **microcontroller projects** without reflashing firmware
* Building **safe, deterministic automation scripts**
* Developing **embedded shell environments**
* Running **Python-like languages** on tiny MCUs using MimiC as a substrate
* Teaching **embedded systems, kernels, and interpreters**

---

## Roadmap

**v0.1** – Core MVP

* Minimal interpreter for expressions, functions, and control flow
* Kernel integration primitives (GPIO, timers, sleep)
* Deterministic memory (GC or region-based)
* Cooperative task support

**v0.2 – v1.0**

* Full module system
* Structs, arrays, and safer pointer abstractions
* REPL / live debugging
* Optional unsafe mode for advanced users
* Higher-level scripting language support (Python/shell layer)

**v2+**

* Multi-platform support (FreeRTOS, STM32, ESP32, RP2040)
* Advanced resource and concurrency management
* Extended standard library

---

## License

MimiC is released under the **MIT License**, encouraging adoption, embedding, and modification while keeping attribution to the original project.

---

## Contribution

Contributors are welcome! Goals for contributions:

* Improving kernel integration
* Expanding standard library primitives
* Language design/syntax proposals
* Porting to new microcontrollers and RTOSes

---

## Contact / Discussion

Ah. 

---

### TL;DR

> **MimiC is a small, safe, deterministic, embeddable scripting language for microcontrollers.**
> Build live scripts, shell layers, or higher-level languages on top — all while keeping your kernel and hardware safe.

---
