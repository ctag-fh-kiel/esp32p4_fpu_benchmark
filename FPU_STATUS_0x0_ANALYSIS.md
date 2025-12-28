# FPU Status Showing 0x0 (OFF) - Analysis and Fix

## The Problem

You're seeing at line 142 (after FPU operations):
```
FPU Status (mstatus[14:13]): 0x0 (OFF - FPU disabled)
```

This should NOT happen after calling `rv_utils_enable_fpu()` and performing FPU operations!

## Root Cause Analysis

### What `rv_utils_enable_fpu()` Actually Does

Looking at `/Users/rma/esp/esp-idf/components/riscv/include/riscv/rv_utils.h` lines 237-248:

```c
FORCE_INLINE_ATTR bool rv_utils_enable_fpu(void)
{
    /* Set mstatus[14:13] to 0b01 */
    RV_SET_CSR(mstatus, CSR_MSTATUS_FPU_ENA);  // Sets bit 13
    
    /* Write to FPU CSR */
    RV_WRITE_CSR(fcsr, 1);
    
    /* Clear bit 13 to make it 0b10 (CLEAN state) */
    RV_CLEAR_CSR(mstatus, CSR_MSTATUS_FPU_CLEAR);  // Clears bit 13
    
    /* Check result is 0b10 (CLEAN) */
    const uint32_t mstatus = RV_READ_CSR(mstatus);
    return ((mstatus >> 13) & 0x3) == 2;  // Should be CLEAN (0b10)
}
```

**Expected result**: mstatus[14:13] = 0b10 (CLEAN = 2)  
**Your actual result**: mstatus[14:13] = 0b00 (OFF = 0)

## Why This Happens

### Theory 1: Context Switch or Interrupt
Printf might be causing a FreeRTOS context switch or interrupt that's disabling the FPU.

### Theory 2: FPU Not Actually Enabled
The enable function might be failing, or there's a hardware/configuration issue.

### Theory 3: Reading Too Late
By the time we read the status (after printf), something has cleared it.

## Changes I Made

### 1. Check FPU Status IMMEDIATELY After Operation

```c
__asm__ volatile (
    "fmul.s %0, %1, %2"
    : "=f"(test_c)
    : "f"(test_a), "f"(test_b)
);

// Read FPU status IMMEDIATELY (before printf can interfere)
uint32_t fpu_state_after_op = read_mstatus_fpu();

printf("  FPU Status immediately after fmul.s: 0x%lx\n", fpu_state_after_op);
```

### 2. Show Raw mstatus Value

```c
static void print_fpu_status(void)
{
    uint32_t mstatus;
    __asm__ volatile ("csrr %0, mstatus" : "=r"(mstatus));
    uint32_t fpu_state = (mstatus >> 13) & 0x3;
    printf("  FPU Status (mstatus[14:13]): 0x%lx (raw mstatus: 0x%08lx)\n", 
           fpu_state, mstatus);
    // ...
}
```

### 3. Track FPU State Changes in Performance Test

```c
for (int iter = 0; iter < NUM_TEST_ITERATIONS; iter++) {
    uint32_t fpu_before = read_mstatus_fpu();
    // ... perform operations ...
    uint32_t fpu_after = read_mstatus_fpu();
    
    printf("  Iteration %d: %lld us (FPU: 0x%lx->0x%lx)\n", 
           iter + 1, elapsed, fpu_before, fpu_after);
}
```

## What to Look For in New Output

### Test 1: Initial Enable
```
After rv_utils_enable_fpu():
  FPU Status (mstatus[14:13]): 0x? (raw mstatus: 0x????????)
```

**Expected**: 0x2 (CLEAN) or 0x1 (INITIAL)  
**If 0x0 (OFF)**: FPU enable is failing!

### Test 2: Immediately After fmul.s
```
  FPU Status immediately after fmul.s: 0x?
```

**Expected**: 0x3 (DIRTY) - proves FPU was actually used  
**If 0x1 (INITIAL)**: FPU instruction didn't mark it dirty  
**If 0x0 (OFF)**: FPU got disabled somehow

### Test 3: After Printf
```
  FPU Status after printf: ...
```

**If this differs from "immediately after"**: Printf is affecting FPU state!

### Test 4: Performance Test Iterations
```
  Iteration 1: XXXXX us (FPU: 0x?->0x?)
```

**Expected**: 0x1->0x3 or 0x2->0x3 (FPU gets marked dirty)  
**If stays at 0x1 or 0x2**: Operations might be optimized away  
**If goes to 0x0**: Something is disabling FPU

## Possible Solutions

### If FPU Enable is Failing (stays at 0x0)

1. **Check hardware configuration**:
   - Is FPU actually enabled in hardware?
   - Check fuses or configuration registers

2. **Try enabling differently**:
   ```c
   // Direct CSR manipulation
   __asm__ volatile ("csrsi mstatus, 0x2000");  // Set bit 13
   ```

### If FPU Gets Disabled After Printf

1. **Disable interrupts during test**:
   ```c
   portDISABLE_INTERRUPTS();
   PERFORM_FPU_OPS(NULL, result);
   uint32_t fpu_state = read_mstatus_fpu();
   portENABLE_INTERRUPTS();
   ```

2. **Use FPU before printf**:
   ```c
   // Keep FPU warm
   volatile float dummy = 1.0f * 2.0f;
   ```

### If FPU Doesn't Get Marked Dirty (stays at INITIAL/CLEAN)

This could mean:
- **Inline assembly isn't actually executing**
- **Compiler is optimizing it away**
- **FPU instructions aren't working as expected**

Try adding more constraints:
```c
__asm__ volatile (
    "fmul.s %0, %1, %2"
    : "=f"(test_c)
    : "f"(test_a), "f"(test_b)
    : "memory"  // Add memory clobber
);
```

## Next Steps

1. **Rebuild and flash** the updated code
2. **Check the new output** focusing on:
   - Raw mstatus values
   - FPU state immediately after fmul.s
   - FPU state transitions (before->after)
3. **Share the output** with these details

## Key Questions to Answer

1. **What is the raw mstatus value?** (should have bits 13 or 14 set)
2. **Does FPU state immediately after fmul.s show DIRTY (0x3)?**
3. **Does printf change the FPU state?**
4. **Do the performance test iterations show FPU state changes?**

Based on the answers, we can determine if:
- FPU is actually being enabled
- FPU instructions are executing
- Something is interfering with FPU state
- The optimization prevention is working

## Bottom Line

**Seeing 0x0 (OFF) after enabling FPU is NOT normal** and indicates either:
1. FPU enable is failing
2. Something is actively disabling it
3. There's a hardware/configuration issue

The new diagnostic output will help us pinpoint exactly where the problem occurs.

