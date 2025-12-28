# ESP32-P4 FPU Architecture Clarification

## Critical Question: Are FPUs Independent or Shared?

### Answer: **INDEPENDENT** - Each core has its own FPU

## Evidence from ESP-IDF Source Code

### 1. RISC-V mstatus CSR Architecture

The FPU is controlled by the **mstatus** Control and Status Register (CSR), which is **per-core** in RISC-V architecture:

```c
// From: components/riscv/include/riscv/rv_utils.h
FORCE_INLINE_ATTR void rv_utils_enable_fpu(void)
{
    /* Set mstatus[14:13] to 0b01 to enable the floating-point unit */
    RV_SET_CSR(mstatus, CSR_MSTATUS_FPU_ENA);
}
```

**Key Point**: Each RISC-V core has its own `mstatus` register. This means:
- Core 0 has mstatus_core0 with its own FPU enable bits [14:13]
- Core 1 has mstatus_core1 with its own FPU enable bits [14:13]
- These are **independent** - enabling FPU on one core doesn't affect the other

### 2. FreeRTOS Context Switch Handling

The FPU state is saved/restored during task context switches **per-core**:

```assembly
// From: components/freertos/FreeRTOS-Kernel/portable/riscv/portasm.S

#if SOC_CPU_HAS_FPU
    // FPU save/restore happens per-task
    fpu_save_regs frame
    // ... switch to new task ...
    fpu_restore_regs frame
#endif
```

**Key Point**: Each task has its own FPU register save area. When tasks switch on different cores, each core saves/restores FPU independently.

### 3. Coprocessor Index

```c
// From soc_caps.h for ESP32-P4
#define SOC_CPU_COPROC_NUM  3  // Multiple coprocessors including FPU
#define SOC_CPU_HAS_FPU     1  // FPU exists per core
```

The coprocessor architecture treats FPU as a per-core resource.

## Comparison with ESP32 (Xtensa)

### ESP32 (Old Xtensa Architecture)
- **SHARED FPU** between cores
- Only one core could use FPU at a time
- Required mutex/locking for FPU access
- FPU enable was global

### ESP32-P4 (RISC-V Architecture)
- **INDEPENDENT FPU** per core
- Both cores can use FPU simultaneously
- No locking needed
- FPU enable is per-core via mstatus CSR

## Why This Matters for Performance

With independent FPUs:
1. **True parallel FP computation** - both cores can do FP math simultaneously
2. **No contention** - no waiting for FPU access
3. **Better multi-core scaling** - expect near 2x performance with dual cores
4. **No synchronization overhead** - each core manages its own FPU state

## Technical Details

### RISC-V FPU State in mstatus[14:13]

| Value | State | Meaning |
|-------|-------|---------|
| 00 | Off | FPU disabled |
| 01 | Initial | FPU enabled, registers clean |
| 10 | Clean | FPU used, registers saved |
| 11 | Dirty | FPU used, registers not yet saved |

Each core has its own mstatus register, so these states are independent.

### FPU Registers (Per Core)

Each core has its own set of:
- 32 floating-point registers (f0-f31)
- FCSR (Floating-Point Control and Status Register)
- Independent from the other core

## Compiler Optimization Concerns

### Problem with -O2

With `-O2` optimization, the compiler may:
1. **Eliminate dead code** - if results aren't used meaningfully
2. **Constant fold** - compute results at compile time
3. **Inline and simplify** - reduce actual FPU instructions executed

### Solutions Implemented

```c
// Use volatile to prevent optimization
volatile float a = 1.5f;
volatile float b = 2.3f;

// Use memory barriers to prevent reordering
__asm__ volatile ("" ::: "memory");

// Use results to prevent elimination
result = a + b + c + d;  // Must be used after macro
```

### Verification Methods

1. **Check mstatus register** - verify FPU is enabled and dirty after operations
2. **Inline assembly** - use explicit FPU instructions that can't be optimized away
3. **Timing variance** - real FPU ops should show consistent timing
4. **Result values** - check results are in expected range (not compile-time constants)

## Updated Test Implementation

The updated benchmark now includes:

1. **FPU Status Checking**
   ```c
   static uint32_t read_mstatus_fpu(void) {
       uint32_t mstatus;
       __asm__ volatile ("csrr %0, mstatus" : "=r"(mstatus));
       return (mstatus >> 13) & 0x3;
   }
   ```

2. **Explicit FPU Instructions**
   ```c
   __asm__ volatile (
       "fmul.s %0, %1, %2"
       : "=f"(test_c)
       : "f"(test_a), "f"(test_b)
   );
   ```

3. **Volatile Variables Throughout**
   - Prevents constant folding
   - Forces memory access
   - Ensures FPU instructions are executed

4. **Memory Barriers**
   - Prevents instruction reordering
   - Ensures operations complete in order

## Expected Results

With properly written benchmark on ESP32-P4:

1. **Single FPU**: ~50-150 MFLOPS (depending on CPU frequency 240-400 MHz)
2. **Dual FPU**: ~2x single FPU (nearly linear scaling)
3. **Independent operation**: Both cores show FPU dirty state simultaneously
4. **No slowdown**: Dual-core FP operations don't slow down compared to single-core

## Conclusion

**The ESP32-P4 has TWO INDEPENDENT FPUs**, one per RISC-V core. This is fundamentally different from the ESP32 (Xtensa) which had a shared FPU. The RISC-V architecture's per-core mstatus CSR design ensures true parallel floating-point computation capability.

## References

- ESP-IDF components/riscv/include/riscv/rv_utils.h
- ESP-IDF components/freertos/FreeRTOS-Kernel/portable/riscv/portasm.S
- ESP-IDF components/soc/esp32p4/include/soc/soc_caps.h
- RISC-V Privileged ISA Specification (mstatus register definition)

