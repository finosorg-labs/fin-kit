/**
 * @file bench_main.c
 * @brief Benchmark runner entry point
 */

#include "bench_framework.h"
#include <stdio.h>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("fin-kit benchmark framework\n");
    printf("NOTE: No benchmarks registered yet.\n");
    printf("      Add benchmark files to benchmarks/ directory to enable tests.\n");
    return 0;
}
