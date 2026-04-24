/**
 * @file bench_codec.c
 * @brief Codec module benchmark entry point
 *
 * This file serves as the main benchmark registration point for the codec module.
 * Individual benchmark modules are in separate files:
 * - bench_fix_codec.c: FIX protocol codec benchmarks
 */

#include "bench_framework.h"
#include <simd_detect.h>
#include <stdio.h>

/* External benchmark functions from sub-modules */
extern void bench_fix_codec_run(void);

/* Entry point for codec benchmarks */
void bench_codec_run(void) {
    printf("\n");
    printf("============================================================\n");
    printf("  Codec Module Performance Benchmarks\n");
    printf("  SIMD level: %s\n", fc_simd_level_string(fc_detect_simd()));
    printf("============================================================\n");

    /* Run all sub-module benchmarks */
    bench_fix_codec_run();

    printf("\n");
    printf("============================================================\n");
    printf("  Codec benchmarks complete\n");
    printf("============================================================\n");
}
