# ESP32-P4 FPU Performance Benchmark

This project implements a comprehensive FPU (Floating Point Unit) performance test for the ESP32-P4 RISC-V microcontroller.

## Overview

The benchmark answers the following key questions about the ESP32-P4's FPU capabilities:

1. **Does the chip have one or two FPUs?**
2. **What is single FPU single-precision floating point performance?**
3. **What is dual FPU single-precision floating point performance?**
4. **What is single FPU performance when accessing PSRAM?**

## Architecture Details

The ESP32-P4 features:
- **Dual-core RISC-V architecture** (2 cores)
- **Two FPUs** - one per core (RISC-V F extension)
- **Single-precision floating point** support
- **32 FPU registers** per core (f0-f31)

## Test Suite

### 1. FPU Count Test
Detects and reports:
- Number of CPU cores
- Number of FPUs (one per core)
- FPU type and capabilities
- Architecture details

### 2. Single FPU Performance Test
Measures single-core FPU performance by:
- Running 1 million FPU operations per iteration
- Performing 10 iterations for averaging
- Calculating MFLOPS (Million Floating Point Operations Per Second)
- Reporting time per operation in nanoseconds

The test performs chained floating-point operations:
- Multiply-add operations (FMADD pattern)
- Uses 4 floating-point variables
- Each iteration = 4 million FLOPs

### 3. Dual FPU Performance Test
Tests parallel FPU execution by:
- Creating tasks pinned to both cores
- Running simultaneous FPU operations on both cores
- Measuring individual core performance
- Calculating combined throughput
- Computing speedup factor

### 4. PSRAM Access Impact Test
Evaluates FPU performance degradation when accessing external PSRAM:
- Allocates buffer in PSRAM
- Performs FPU operations with PSRAM writes
- Compares with internal RAM performance
- Reports slowdown factor and performance loss percentage

## Building the Project

### Prerequisites
- ESP-IDF v5.5 or later
- ESP32-P4 target configured
- PSRAM enabled (for PSRAM test)

### Build Steps

1. Set the target to ESP32-P4:
```bash
cd /Users/rma/esp/src/p4fpu_benchmark
idf.py set-target esp32p4
```

2. Configure the project (optional):
```bash
idf.py menuconfig
```

Key configurations:
- Component config -> FreeRTOS -> "Run FreeRTOS only on first core" = **NO** (for dual-core test)
- Component config -> ESP PSRAM -> Enable PSRAM (for PSRAM test)

3. Build:
```bash
idf.py build
```

4. Flash and monitor:
```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

## Expected Output

```
========================================
ESP32-P4 FPU PERFORMANCE BENCHMARK
========================================
ESP-IDF Version: v5.5.1
Chip revision: 0
CPU Frequency: 360 MHz

========================================
FPU COUNT TEST
========================================
Chip: ESP32-P4
Cores: 2
Features: 0x00000000

FPU Support: YES

RISC-V Architecture Details:
- Each RISC-V core has its own FPU
- Total FPUs: 2 (one per core)
- FPU Type: Single-precision floating point (F extension)
- FPU Registers: 32 (f0-f31)

Conclusion: The ESP32-P4 has 2 FPUs (one per RISC-V core)

========================================
SINGLE FPU PERFORMANCE TEST
========================================
Running 10 iterations of 1000000 FPU operations each...
  Iteration 1: XXXXX us
  ...
Results:
  Average time: XXXXX us
  Operations: 1000000 x 4 = 4000000 FLOPs
  Performance: XX.XX MFLOPS
  Time per operation: XX.XX ns

========================================
DUAL FPU PERFORMANCE TEST (BOTH CORES)
========================================
Creating task on Core 1...
Core 0: Starting FPU test on core 0
Core 1: Starting FPU test on core 1
Core 0: FPU test complete
Core 1: FPU test complete

Results:
  Core 0 time: XXXXX us (XX.XX MFLOPS)
  Core 1 time: XXXXX us (XX.XX MFLOPS)
  Total time: XXXXX us
  Combined performance: XX.XX MFLOPS
  Speedup: X.XXx

========================================
FPU PERFORMANCE WITH PSRAM ACCESS TEST
========================================
PSRAM detected: XXXXXXXX bytes
Allocated 4096 bytes in PSRAM at address: 0xXXXXXXXX
Running 10 iterations with PSRAM access...

Results (with PSRAM access):
  Average time: XXXXX us
  Performance: XX.XX MFLOPS

Results (without PSRAM access):
  Average time: XXXXX us
  Performance: XX.XX MFLOPS

PSRAM Impact:
  Slowdown factor: X.XXx
  Performance loss: XX.XX%

========================================
ALL TESTS COMPLETED
========================================
```

## Implementation Details

### Key Files
- `main/hello_world_main.c` - Main benchmark implementation
- `main/CMakeLists.txt` - Component configuration

### Dependencies
The project requires the following ESP-IDF components:
- `esp_timer` - High-resolution timing
- `esp_hw_support` - CPU frequency and hardware support
- `riscv` - RISC-V specific functions (FPU enable)
- `spi_flash` - Flash memory access
- `heap` - Memory allocation (PSRAM support)

### FPU Enablement
The benchmark explicitly enables the FPU on each core using:
```c
rv_utils_enable_fpu();
```

This sets the appropriate RISC-V mstatus CSR bits to enable floating-point operations.

### Timing
All timing measurements use `esp_timer_get_time()` which provides microsecond resolution.

## Performance Expectations

Typical performance characteristics for ESP32-P4:

1. **Single FPU**: ~XX-XX MFLOPS (depends on CPU frequency)
2. **Dual FPU**: ~2x single FPU performance (nearly linear scaling)
3. **PSRAM Impact**: ~2-4x slowdown compared to internal RAM access

Note: Actual performance depends on:
- CPU frequency (configurable: 240-400 MHz)
- Cache efficiency
- Memory access patterns
- Compiler optimizations

## Troubleshooting

### Build Errors
- Ensure target is set to `esp32p4`
- Check that all required components are in PRIV_REQUIRES
- Verify ESP-IDF version is 5.5 or later

### Runtime Issues
- If dual-core test fails: Check FreeRTOS is configured for dual-core mode
- If PSRAM test fails: Verify PSRAM is enabled and detected
- If FPU operations fail: Ensure FPU is properly enabled in hardware

## License

This benchmark is provided under the CC0-1.0 license.

## Author

Created for ESP32-P4 FPU performance evaluation.

