# MANUAL TEST GUIDE - ESP32-P4 FPU Benchmark

## Step 1: Flash the Device

Run this command in your terminal:

```bash
cd /Users/rma/esp/src/p4fpu_benchmark
source /Users/rma/esp/esp-idf/export.sh
idf.py flash monitor
```

## Step 2: Expected Output Analysis

### SECTION 1: FPU Count Test

**Look for this:**
```
========================================
FPU COUNT TEST
========================================
Chip: ESP32-P4
Cores: 2
Features: 0x????????

FPU Support: YES

RISC-V Architecture Analysis:
- FPU control: Per-core mstatus CSR (independent)
- FPU registers: 32 per core (f0-f31)
- FPU state: Saved during task context switches
- Coprocessor index: 3
  FPU Status (mstatus[14:13]): 0x1 (INITIAL - FPU enabled, registers clean)
  FPU Test: 1.5 * 2.3 = 3.450000 (Expected: 3.45)
  FPU Status (mstatus[14:13]): 0x3 (DIRTY - FPU used, registers not saved)

Conclusion:
- The ESP32-P4 has 2 INDEPENDENT FPUs
- Each RISC-V core has its own FPU hardware
- FPU state is managed per-core via mstatus CSR
- FPUs are NOT shared between cores
```

**✅ VERIFICATION CHECKLIST:**
- [ ] FPU Status changes from 0x1 (INITIAL) to 0x3 (DIRTY)
- [ ] FPU Test result is 3.45 (correct multiplication)
- [ ] Shows 2 cores and 2 independent FPUs
- [ ] Coprocessor index is 3

**❌ FAILURE INDICATORS:**
- FPU status stays at 0x1 (operations were optimized away)
- FPU Test result is 0.0 or wrong value
- Crash or exception

---

### SECTION 2: Single FPU Performance Test

**Look for this:**
```
========================================
SINGLE FPU PERFORMANCE TEST
========================================
Running on core: 0
  FPU Status (mstatus[14:13]): 0x1 (INITIAL - FPU enabled, registers clean)

Running 10 iterations of 1000000 FPU operations each...
  Iteration 1: 45000 us
  Iteration 2: 45100 us
  Iteration 3: 45050 us
  ...

After FPU operations:
  FPU Status (mstatus[14:13]): 0x3 (DIRTY - FPU used, registers not saved)

Results:
  Average time: ~45000 us
  Operations: 1000000 x 4 = 4000000 FLOPs
  Performance: 80-100 MFLOPS
  Time per operation: 10-15 ns
  Result (to prevent optimization): [some large float value]
```

**✅ VERIFICATION CHECKLIST:**
- [ ] FPU Status changes from INITIAL to DIRTY
- [ ] Times are consistent (within 5-10% variance)
- [ ] Times are reasonable (not zero, not identical)
- [ ] Performance is 50-150 MFLOPS (depending on CPU freq)
- [ ] Result is a float value (not 0, not NaN)

**❌ FAILURE INDICATORS:**
- All iterations show exactly the same time (optimized away)
- Time is very short (< 1000 us) - likely optimized
- FPU status doesn't change to DIRTY
- Performance > 500 MFLOPS (unrealistic, likely optimized)

---

### SECTION 3: Dual FPU Performance Test

**Look for this:**
```
========================================
DUAL FPU PERFORMANCE TEST (BOTH CORES)
========================================
Creating task on Core 1...
Core 1: Starting FPU test on core 1
  FPU Status (mstatus[14:13]): 0x1 (INITIAL - FPU enabled, registers clean)
Core 0: Starting FPU test on core 0
  FPU Status (mstatus[14:13]): 0x1 (INITIAL - FPU enabled, registers clean)
Core 0: FPU test complete, result = [float value]
  FPU Status (mstatus[14:13]): 0x3 (DIRTY - FPU used, registers not saved)
Core 1: FPU test complete, result = [float value]
  FPU Status (mstatus[14:13]): 0x3 (DIRTY - FPU used, registers not saved)

Results:
  Core 0 time: ~45000 us (85-95 MFLOPS)
  Core 1 time: ~45000 us (85-95 MFLOPS)
  Total time: ~45000 us
  Combined performance: 170-190 MFLOPS
  Speedup: 1.9x-2.0x
```

**✅ VERIFICATION CHECKLIST:**
- [ ] Both cores show FPU status change (INITIAL → DIRTY)
- [ ] Core 0 and Core 1 times are similar
- [ ] Combined performance is ~2x single core
- [ ] Speedup is close to 2.0x
- [ ] Both cores show independent FPU states

**❌ FAILURE INDICATORS:**
- Only one core shows DIRTY state (FPU sharing?)
- Speedup is 1.0x (serialization, not parallel)
- One core is much slower than the other
- Speedup > 2.5x (unrealistic)

**🔍 KEY INSIGHT:**
If both cores can simultaneously show DIRTY state and achieve ~2x performance, this PROVES the FPUs are independent!

---

### SECTION 4: PSRAM Access Test

**Look for this:**
```
========================================
FPU PERFORMANCE WITH PSRAM ACCESS TEST
========================================
PSRAM detected: 8388608 bytes
Allocated 4096 bytes in PSRAM at address: 0x48??????

Running 10 iterations with PSRAM access...
  Iteration 1: ~120000 us
  ...

Results (with PSRAM access):
  Average time: ~120000 us
  Performance: 30-40 MFLOPS

Results (without PSRAM access):
  Average time: ~45000 us
  Performance: 85-95 MFLOPS

PSRAM Impact:
  Slowdown factor: 2.5x-3.5x
  Performance loss: 150-250%
```

**✅ VERIFICATION CHECKLIST:**
- [ ] PSRAM is detected (size shown)
- [ ] PSRAM allocated successfully
- [ ] With PSRAM is 2-4x slower than without
- [ ] Slowdown is reasonable and consistent

**❌ FAILURE INDICATORS:**
- PSRAM not detected (not enabled in config)
- No performance difference (PSRAM not actually used)
- Crash during allocation

---

## Optimization Verification Summary

### If -O2 optimization was properly prevented:

1. **FPU State Changes**: All tests show INITIAL → DIRTY
2. **Consistent Timing**: Small variance between iterations (< 10%)
3. **Reasonable Performance**: 50-150 MFLOPS range for 240-400 MHz CPU
4. **Working Results**: Float values are computed correctly
5. **Dual-Core Scaling**: ~2x performance improvement

### If operations were optimized away:

1. **FPU State Static**: Stays at INITIAL or OFF
2. **Suspiciously Fast**: < 1000 us or identical timing
3. **Unrealistic Performance**: > 500 MFLOPS
4. **Zero/Constant Results**: Results are 0 or compile-time constants

---

## Quick Diagnosis

### Everything Working ✅
```
FPU Status: INITIAL → DIRTY ✅
Timing: ~45ms per test ✅
Performance: ~88 MFLOPS ✅
Dual-core speedup: 1.95x ✅
PSRAM slowdown: 2.8x ✅
```
**Conclusion**: FPU benchmark is working correctly. FPUs are independent!

### Optimization Problem ❌
```
FPU Status: INITIAL (no change) ❌
Timing: 0-100 us ❌
Performance: > 500 MFLOPS ❌
Results: 0.000000 ❌
```
**Conclusion**: Operations optimized away. Need more anti-optimization measures.

### FPU Sharing? 🤔
```
Single core: 88 MFLOPS ✅
Dual core speedup: 1.0x ❌
Core 1 shows: INITIAL (no DIRTY) ❌
```
**Conclusion**: Possible FPU contention or serialization issue.

---

## What to Report Back

Please share:

1. **Full console output** (especially the FPU status lines)
2. **Performance numbers** from all 4 tests
3. **Any errors or crashes**
4. **FPU status values** (0x1, 0x2, 0x3)
5. **Speedup factor** from dual-core test

Based on this, I can tell you:
- ✅ If optimizations were properly prevented
- ✅ If FPUs are truly independent
- ✅ If performance is as expected
- ✅ Any issues that need fixing

---

## File Location

The updated test code is at:
`/Users/rma/esp/src/p4fpu_benchmark/main/hello_world_main.c`

To re-flash if needed:
```bash
cd /Users/rma/esp/src/p4fpu_benchmark
idf.py build flash monitor
```

