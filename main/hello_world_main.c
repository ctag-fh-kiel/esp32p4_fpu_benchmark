/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_cpu.h"
#include "esp_heap_caps.h"
#include "esp_private/esp_clk.h"
#include "riscv/rv_utils.h"

// Number of FPU operations for testing
#define FPU_OPS_COUNT 1000000

// Test configuration
#define NUM_TEST_ITERATIONS 10

// FPU register count (RISC-V has 32 FP registers)
#define FPU_REG_COUNT 32

// Macro to perform FPU operations
#define PERFORM_FPU_OPS(ptr, result) do { \
    float a = 1.5f; \
    float b = 2.3f; \
    float c = 3.7f; \
    float d = 4.2f; \
    for (volatile int i = 0; i < FPU_OPS_COUNT; i++) { \
        a = a * b + c; \
        b = b * c + d; \
        c = c * d + a; \
        d = d * a + b; \
        if (ptr != NULL) { \
            ((float*)(ptr))[i % 1024] = a; \
        } \
    } \
    result = a + b + c + d; \
} while(0)

// Global variables for dual-core test
static volatile float core0_result = 0.0f;
static volatile float core1_result = 0.0f;
static volatile bool core0_done = false;
static volatile bool core1_done = false;
static volatile int64_t core0_start_time = 0;
static volatile int64_t core1_start_time = 0;
static volatile int64_t core0_end_time = 0;
static volatile int64_t core1_end_time = 0;

// Task for Core 1 FPU test
static void core1_fpu_task(void *arg)
{
    // Enable FPU on this core
    rv_utils_enable_fpu();

    printf("Core 1: Starting FPU test on core %d\n", esp_cpu_get_core_id());

    // Wait a bit to synchronize with core 0
    vTaskDelay(pdMS_TO_TICKS(100));

    // Perform FPU operations
    core1_start_time = esp_timer_get_time();
    PERFORM_FPU_OPS(NULL, core1_result);
    core1_end_time = esp_timer_get_time();

    core1_done = true;
    printf("Core 1: FPU test complete, result = %f\n", core1_result);

    vTaskDelete(NULL);
}

// Function to test if the chip has one or two FPUs
static void test_fpu_count(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    printf("\n========================================\n");
    printf("FPU COUNT TEST\n");
    printf("========================================\n");
    printf("Chip: ESP32-P4\n");
    printf("Cores: %d\n", chip_info.cores);
    printf("Features: 0x%08lx\n", chip_info.features);

#if SOC_CPU_HAS_FPU
    printf("FPU Support: YES\n");
    printf("\nRISC-V Architecture Details:\n");
    printf("- Each RISC-V core has its own FPU\n");
    printf("- Total FPUs: %d (one per core)\n", chip_info.cores);
    printf("- FPU Type: Single-precision floating point (F extension)\n");
    printf("- FPU Registers: 32 (f0-f31)\n");
#else
    printf("FPU Support: NO\n");
#endif

    printf("\nConclusion: The ESP32-P4 has %d FPU%s (one per RISC-V core)\n",
           chip_info.cores, chip_info.cores > 1 ? "s" : "");
}

// Function to test single FPU performance (single precision)
static void test_single_fpu_performance(void)
{
    printf("\n========================================\n");
    printf("SINGLE FPU PERFORMANCE TEST\n");
    printf("========================================\n");

    // Enable FPU
    rv_utils_enable_fpu();

    float result = 0.0f;
    int64_t total_time = 0;

    printf("Running %d iterations of %d FPU operations each...\n",
           NUM_TEST_ITERATIONS, FPU_OPS_COUNT);

    for (int iter = 0; iter < NUM_TEST_ITERATIONS; iter++) {
        int64_t start = esp_timer_get_time();
        PERFORM_FPU_OPS(NULL, result);
        int64_t end = esp_timer_get_time();

        int64_t elapsed = end - start;
        total_time += elapsed;
        printf("  Iteration %d: %lld us\n", iter + 1, elapsed);
    }

    int64_t avg_time = total_time / NUM_TEST_ITERATIONS;
    float mflops = (float)(FPU_OPS_COUNT * 4) / (float)avg_time;

    printf("\nResults:\n");
    printf("  Average time: %lld us\n", avg_time);
    printf("  Operations: %d x 4 = %d FLOPs\n", FPU_OPS_COUNT, FPU_OPS_COUNT * 4);
    printf("  Performance: %.2f MFLOPS\n", mflops);
    printf("  Time per operation: %.2f ns\n", (float)avg_time * 1000.0f / (float)(FPU_OPS_COUNT * 4));
    printf("  Result (to prevent optimization): %f\n", result);
}

// Function to test dual FPU performance (both cores)
static void test_dual_fpu_performance(void)
{
    printf("\n========================================\n");
    printf("DUAL FPU PERFORMANCE TEST (BOTH CORES)\n");
    printf("========================================\n");

#if CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE
    printf("ERROR: This test requires dual-core mode!\n");
    printf("Please configure menuconfig: Component config -> FreeRTOS -> Run FreeRTOS only on first core = NO\n");
    return;
#else
    // Enable FPU on core 0
    rv_utils_enable_fpu();

    printf("Creating task on Core 1...\n");

    // Reset flags
    core0_done = false;
    core1_done = false;

    // Create task on Core 1
    TaskHandle_t core1_task;
    xTaskCreatePinnedToCore(core1_fpu_task, "core1_fpu", 4096, NULL, 5, &core1_task, 1);

    // Small delay to let core 1 initialize
    vTaskDelay(pdMS_TO_TICKS(100));

    printf("Core 0: Starting FPU test on core %d\n", esp_cpu_get_core_id());

    // Perform FPU operations on core 0
    core0_start_time = esp_timer_get_time();
    PERFORM_FPU_OPS(NULL, core0_result);
    core0_end_time = esp_timer_get_time();

    core0_done = true;
    printf("Core 0: FPU test complete, result = %f\n", core0_result);

    // Wait for core 1 to finish
    while (!core1_done) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    int64_t core0_time = core0_end_time - core0_start_time;
    int64_t core1_time = core1_end_time - core1_start_time;
    int64_t total_start = (core0_start_time < core1_start_time) ? core0_start_time : core1_start_time;
    int64_t total_end = (core0_end_time > core1_end_time) ? core0_end_time : core1_end_time;
    int64_t total_time = total_end - total_start;

    float core0_mflops = (float)(FPU_OPS_COUNT * 4) / (float)core0_time;
    float core1_mflops = (float)(FPU_OPS_COUNT * 4) / (float)core1_time;
    float total_mflops = (float)(FPU_OPS_COUNT * 4 * 2) / (float)total_time;

    printf("\nResults:\n");
    printf("  Core 0 time: %lld us (%.2f MFLOPS)\n", core0_time, core0_mflops);
    printf("  Core 1 time: %lld us (%.2f MFLOPS)\n", core1_time, core1_mflops);
    printf("  Total time: %lld us\n", total_time);
    printf("  Combined performance: %.2f MFLOPS\n", total_mflops);
    printf("  Speedup: %.2fx\n", (core0_mflops + core1_mflops) / ((core0_mflops + core1_mflops) / 2.0f));
#endif
}

// Function to test FPU performance with PSRAM access
static void test_fpu_performance_with_psram(void)
{
    printf("\n========================================\n");
    printf("FPU PERFORMANCE WITH PSRAM ACCESS TEST\n");
    printf("========================================\n");

#if !CONFIG_SPIRAM
    printf("ERROR: PSRAM is not enabled!\n");
    printf("Please enable PSRAM in menuconfig: Component config -> ESP PSRAM\n");
    return;
#else

    // Check if PSRAM is available
    size_t psram_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    if (psram_size == 0) {
        printf("ERROR: PSRAM not detected or not available!\n");
        return;
    }

    printf("PSRAM detected: %zu bytes\n", psram_size);

    // Allocate buffer in PSRAM
    size_t buffer_size = 1024 * sizeof(float);
    float *psram_buffer = (float *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);

    if (psram_buffer == NULL) {
        printf("ERROR: Failed to allocate PSRAM buffer!\n");
        return;
    }

    // Initialize buffer
    memset(psram_buffer, 0, buffer_size);

    printf("Allocated %zu bytes in PSRAM at address: %p\n", buffer_size, psram_buffer);

    // Enable FPU
    rv_utils_enable_fpu();

    // Test with PSRAM access
    float result = 0.0f;
    int64_t total_time = 0;

    printf("Running %d iterations with PSRAM access...\n", NUM_TEST_ITERATIONS);

    for (int iter = 0; iter < NUM_TEST_ITERATIONS; iter++) {
        int64_t start = esp_timer_get_time();
        PERFORM_FPU_OPS(psram_buffer, result);
        int64_t end = esp_timer_get_time();

        int64_t elapsed = end - start;
        total_time += elapsed;
        printf("  Iteration %d: %lld us\n", iter + 1, elapsed);
    }

    int64_t avg_time = total_time / NUM_TEST_ITERATIONS;
    float mflops = (float)(FPU_OPS_COUNT * 4) / (float)avg_time;

    printf("\nResults (with PSRAM access):\n");
    printf("  Average time: %lld us\n", avg_time);
    printf("  Operations: %d x 4 = %d FLOPs\n", FPU_OPS_COUNT, FPU_OPS_COUNT * 4);
    printf("  Performance: %.2f MFLOPS\n", mflops);
    printf("  Time per operation: %.2f ns\n", (float)avg_time * 1000.0f / (float)(FPU_OPS_COUNT * 4));
    printf("  Result (to prevent optimization): %f\n", result);

    // Also run test without PSRAM for comparison
    printf("\nRunning comparison test without PSRAM access...\n");
    total_time = 0;
    for (int iter = 0; iter < NUM_TEST_ITERATIONS; iter++) {
        int64_t start = esp_timer_get_time();
        PERFORM_FPU_OPS(NULL, result);
        int64_t end = esp_timer_get_time();
        total_time += (end - start);
    }

    int64_t avg_time_no_psram = total_time / NUM_TEST_ITERATIONS;
    float mflops_no_psram = (float)(FPU_OPS_COUNT * 4) / (float)avg_time_no_psram;

    printf("Results (without PSRAM access):\n");
    printf("  Average time: %lld us\n", avg_time_no_psram);
    printf("  Performance: %.2f MFLOPS\n", mflops_no_psram);

    printf("\nPSRAM Impact:\n");
    printf("  Slowdown factor: %.2fx\n", (float)avg_time / (float)avg_time_no_psram);
    printf("  Performance loss: %.2f%%\n",
           ((float)(avg_time - avg_time_no_psram) / (float)avg_time_no_psram) * 100.0f);

    // Free PSRAM buffer
    heap_caps_free(psram_buffer);
#endif
}

void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("ESP32-P4 FPU PERFORMANCE BENCHMARK\n");
    printf("========================================\n");

    // Get chip information
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    printf("ESP-IDF Version: %s\n", esp_get_idf_version());
    printf("Chip revision: %d\n", chip_info.revision);
    printf("CPU Frequency: %d MHz\n", esp_clk_cpu_freq() / 1000000);

    // Run all tests
    test_fpu_count();

    vTaskDelay(pdMS_TO_TICKS(1000));
    test_single_fpu_performance();

    vTaskDelay(pdMS_TO_TICKS(1000));
    test_dual_fpu_performance();

    vTaskDelay(pdMS_TO_TICKS(1000));
    test_fpu_performance_with_psram();

    printf("\n========================================\n");
    printf("ALL TESTS COMPLETED\n");
    printf("========================================\n");
}
