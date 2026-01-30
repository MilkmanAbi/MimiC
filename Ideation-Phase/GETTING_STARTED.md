# MimiC - Getting Started

## Building the Project

### Prerequisites

1. **Pico SDK** (version 2.0.0 or later)
   ```bash
   git clone https://github.com/raspberrypi/pico-sdk.git
   cd pico-sdk
   git submodule update --init
   export PICO_SDK_PATH=$(pwd)
   ```

2. **ARM Toolchain**
   ```bash
   # Ubuntu/Debian
   sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential

   # macOS
   brew install cmake
   brew install --cask gcc-arm-embedded
   ```

3. **FreeRTOS Kernel**
   ```bash
   cd mimic/lib
   git clone https://github.com/FreeRTOS/FreeRTOS-Kernel.git
   ```

4. **TCC (Tiny C Compiler)**
   ```bash
   cd mimic/lib
   git clone https://repo.or.cz/tinycc.git tcc
   ```

5. **FatFS**
   ```bash
   cd mimic/lib
   git clone https://github.com/abbrev/fatfs.git
   ```

### Build Steps

```bash
cd mimic
mkdir build
cd build

# Configure
cmake ..

# Build
make -j$(nproc)

# Flash to Pico
# 1. Hold BOOTSEL button on Pico
# 2. Plug in USB cable
# 3. Copy UF2 file:
cp mimic.uf2 /media/$USER/RPI-RP2/
```

## Preparing the SD Card

1. **Format SD card as FAT32**

2. **Create directory structure:**
   ```
   /
   ├── include/          (Pico SDK headers)
   ├── examples/         (Example programs)
   └── user/             (Your programs)
   ```

3. **Copy SDK headers:**
   ```bash
   # Copy simplified headers from mimic/sdk-headers/
   cp -r sdk-headers/* /media/$USER/SDCARD/include/
   ```

4. **Copy examples:**
   ```bash
   cp examples/* /media/$USER/SDCARD/examples/
   ```

## Using MimiC

### Via UART Shell

1. **Connect to Pico via USB**
   ```bash
   # Linux
   screen /dev/ttyACM0 115200

   # macOS
   screen /dev/tty.usbmodem* 115200
   ```

2. **Available commands:**
   ```
   mimic> help                    # Show help
   mimic> compile /examples/blink.c   # Compile program
   mimic> run                     # Run compiled program
   mimic> stop                    # Stop running program
   mimic> ls /examples            # List files
   mimic> cat /examples/blink.c   # Show file contents
   mimic> symbols                 # Show symbol table
   mimic> mem                     # Show memory usage
   ```

### Writing Your Own Programs

Create a file on the SD card (e.g. `/user/myapp.c`):

```c
#include <pico/stdlib.h>

int main() {
    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);
    
    while (1) {
        gpio_put(25, 1);
        sleep_ms(1000);
        gpio_put(25, 0);
        sleep_ms(1000);
    }
    
    return 0;
}
```

Then compile and run:
```
mimic> compile /user/myapp.c
mimic> run
```

## Current Limitations (v0.1)

- ⚠️ TCC port not yet complete - compilation will fail
- ⚠️ SD card support not implemented - use UART for now
- ⚠️ Symbol table is minimal - only basic GPIO/time functions
- ⚠️ No debugging support
- ⚠️ Single program execution only

## Next Steps

See [ROADMAP.md](ROADMAP.md) for planned features and development timeline.

## Troubleshooting

### "SD card not detected"
- Check wiring (CS=GPIO5, SCK=GPIO2, MOSI=GPIO3, MISO=GPIO4)
- Ensure SD card is FAT32 formatted
- Try different SD card (some cards are incompatible)

### "Compilation failed"
- TCC port is incomplete in v0.1
- This is expected - compiler integration is in progress

### "Out of memory"
- Reduce program size
- Check for memory leaks in user code
- Increase FreeRTOS heap size in FreeRTOSConfig.h

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for development guidelines.
