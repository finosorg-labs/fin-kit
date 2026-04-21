/**
 * @file bench_matrix.c
 * @brief Performance benchmarks for matrix operations
 *
 * Measures GEMM, GEMV, Transpose, and vector operations at various sizes.
 * Reports throughput in GFLOPS and GB/s.
 */

#include "bench_framework.h"
#include <fin-kit/matrix/matrix.h>
#include <fin-kit/platform/simd_detect.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Benchmark data structures */

typedef struct {
    int m, n, k;
    double *A, *B, *C;
} gemm_data_t;

typedef struct {
    int m, n;
    double *A, *x, *y;
} gemv_data_t;

typedef struct {
    int rows, cols;
    double *src, *dst;
} transpose_data_t;

typedef struct {
    int n;
    double *x, *y;
} vec_data_t;

/* Helper: fill array with pseudo-random values in [0, 1) */
static void fill_random(double* arr, int n) {
    unsigned int seed = 42;
    for (int i = 0; i < n; i++) {
        seed = seed * 1103515245 + 12345;
        arr[i] = (double)(seed & 0x7fffffff) / 2147483648.0;
    }
}

/* GEMM benchmark function */
static void bench_gemm_fn(void* user_data) {
    gemm_data_t* d = (gemm_data_t*)user_data;
    fc_mat_gemm_f64(d->m, d->n, d->k, 1.0,
                    d->A, d->k, d->B, d->n, 0.0, d->C, d->n);
}

/* GEMV benchmark function */
static void bench_gemv_fn(void* user_data) {
    gemv_data_t* d = (gemv_data_t*)user_data;
    fc_mat_gemv_f64(d->m, d->n, 1.0,
                    d->A, d->n, d->x, 0.0, d->y);
}

/* Transpose benchmark function */
static void bench_transpose_fn(void* user_data) {
    transpose_data_t* d = (transpose_data_t*)user_data;
    fc_mat_transpose_f64(d->rows, d->cols,
                         d->src, d->cols, d->dst, d->rows);
}

/* VecDot benchmark function */
static void bench_vec_dot_fn(void* user_data) {
    vec_data_t* d = (vec_data_t*)user_data;
    double result;
    fc_vec_dot_f64(d->x, d->y, d->n, &result);
}

/* VecNormL2 benchmark function */
static void bench_vec_norm_l2_fn(void* user_data) {
    vec_data_t* d = (vec_data_t*)user_data;
    double result;
    fc_vec_norm_l2_f64(d->x, d->n, &result);
}

/* Run GEMM benchmarks at various sizes */
static void run_gemm_benchmarks(void) {
    printf("\n");
    printf("GEMM Benchmarks (C = A*B, double precision)\n");
    printf("------------------------------------------------------------\n");

    int sizes[] = {16, 32, 64, 128, 256, 512};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int s = 0; s < num_sizes; s++) {
        int n = sizes[s];
        gemm_data_t data;
        data.m = n;
        data.n = n;
        data.k = n;
        data.A = (double*)malloc(n * n * sizeof(double));
        data.B = (double*)malloc(n * n * sizeof(double));
        data.C = (double*)malloc(n * n * sizeof(double));

        fill_random(data.A, n * n);
        fill_random(data.B, n * n);
        memset(data.C, 0, n * n * sizeof(double));

        char name[64];
        snprintf(name, sizeof(name), "GEMM %dx%d", n, n);

        fc_bench_config_t config = FC_BENCH_CONFIG_DEFAULT;
        config.name = name;
        config.data_size = 3 * n * n * sizeof(double);
        config.min_time_ms = 200.0;
        config.warmup_ms = 50.0;

        fc_bench_result_t result;
        fc_bench_run(&config, bench_gemm_fn, &data, &result);

        /* GEMM FLOPS = 2*m*n*k */
        double flops = 2.0 * n * n * n;
        result.gflops = fc_bench_gflops(flops * result.iterations,
                                        result.elapsed_ms);
        printf("  %s: %.2f GFLOPS, %.2f ns/iter\n",
               name, result.gflops, result.mean_ns);

        free(data.A);
        free(data.B);
        free(data.C);
    }
}

/* Run GEMV benchmarks */
static void run_gemv_benchmarks(void) {
    printf("\n");
    printf("GEMV Benchmarks (y = A*x, double precision)\n");
    printf("------------------------------------------------------------\n");

    int sizes[] = {64, 128, 256, 512, 1024};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int s = 0; s < num_sizes; s++) {
        int n = sizes[s];
        gemv_data_t data;
        data.m = n;
        data.n = n;
        data.A = (double*)malloc(n * n * sizeof(double));
        data.x = (double*)malloc(n * sizeof(double));
        data.y = (double*)malloc(n * sizeof(double));

        fill_random(data.A, n * n);
        fill_random(data.x, n);
        memset(data.y, 0, n * sizeof(double));

        char name[64];
        snprintf(name, sizeof(name), "GEMV %dx%d", n, n);

        fc_bench_config_t config = FC_BENCH_CONFIG_DEFAULT;
        config.name = name;
        config.data_size = (n * n + 2 * n) * sizeof(double);
        config.min_time_ms = 200.0;
        config.warmup_ms = 50.0;

        fc_bench_result_t result;
        fc_bench_run(&config, bench_gemv_fn, &data, &result);

        /* GEMV FLOPS = 2*m*n */
        double flops = 2.0 * n * n;
        result.gflops = fc_bench_gflops(flops * result.iterations,
                                        result.elapsed_ms);
        printf("  %s: %.2f GFLOPS, %.2f ns/iter\n",
               name, result.gflops, result.mean_ns);

        free(data.A);
        free(data.x);
        free(data.y);
    }
}

/* Run Transpose benchmarks */
static void run_transpose_benchmarks(void) {
    printf("\n");
    printf("Transpose Benchmarks (double precision)\n");
    printf("------------------------------------------------------------\n");

    int sizes[] = {64, 128, 256, 512, 1024};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int s = 0; s < num_sizes; s++) {
        int n = sizes[s];
        transpose_data_t data;
        data.rows = n;
        data.cols = n;
        data.src = (double*)malloc(n * n * sizeof(double));
        data.dst = (double*)malloc(n * n * sizeof(double));

        fill_random(data.src, n * n);

        char name[64];
        snprintf(name, sizeof(name), "Transpose %dx%d", n, n);

        fc_bench_config_t config = FC_BENCH_CONFIG_DEFAULT;
        config.name = name;
        config.data_size = 2 * n * n * sizeof(double);
        config.min_time_ms = 200.0;
        config.warmup_ms = 50.0;

        fc_bench_result_t result;
        fc_bench_run(&config, bench_transpose_fn, &data, &result);

        double bytes = 2.0 * n * n * sizeof(double);
        result.throughput_gb_s = fc_bench_throughput_gb_s(
            (size_t)bytes, result.elapsed_ms / result.iterations);
        printf("  %s: %.2f GB/s, %.2f ns/iter\n",
               name, result.throughput_gb_s, result.mean_ns);

        free(data.src);
        free(data.dst);
    }
}

/* Run vector operation benchmarks */
static void run_vector_benchmarks(void) {
    printf("\n");
    printf("Vector Operation Benchmarks (double precision)\n");
    printf("------------------------------------------------------------\n");

    int sizes[] = {256, 1024, 4096, 16384, 65536};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int s = 0; s < num_sizes; s++) {
        int n = sizes[s];
        vec_data_t data;
        data.n = n;
        data.x = (double*)malloc(n * sizeof(double));
        data.y = (double*)malloc(n * sizeof(double));

        fill_random(data.x, n);
        fill_random(data.y, n);

        /* VecDot */
        {
            char name[64];
            snprintf(name, sizeof(name), "VecDot n=%d", n);

            fc_bench_config_t config = FC_BENCH_CONFIG_DEFAULT;
            config.name = name;
            config.data_size = 2 * n * sizeof(double);
            config.min_time_ms = 200.0;
            config.warmup_ms = 50.0;

            fc_bench_result_t result;
            fc_bench_run(&config, bench_vec_dot_fn, &data, &result);

            /* Dot product: 2n FLOPS (n multiplies + n adds) */
            double flops = 2.0 * n;
            result.gflops = fc_bench_gflops(flops * result.iterations,
                                            result.elapsed_ms);
            printf("  %s: %.2f GFLOPS, %.2f ns/iter\n",
                   name, result.gflops, result.mean_ns);
        }

        /* VecNormL2 */
        {
            char name[64];
            snprintf(name, sizeof(name), "VecNormL2 n=%d", n);

            fc_bench_config_t config = FC_BENCH_CONFIG_DEFAULT;
            config.name = name;
            config.data_size = n * sizeof(double);
            config.min_time_ms = 200.0;
            config.warmup_ms = 50.0;

            fc_bench_result_t result;
            fc_bench_run(&config, bench_vec_norm_l2_fn, &data, &result);

            double flops = 2.0 * n + 1; /* n mul + n add + 1 sqrt */
            result.gflops = fc_bench_gflops(flops * result.iterations,
                                            result.elapsed_ms);
            printf("  %s: %.2f GFLOPS, %.2f ns/iter\n",
                   name, result.gflops, result.mean_ns);
        }

        free(data.x);
        free(data.y);
    }
}

/* Entry point for matrix benchmarks */
void bench_matrix_run(void) {
    printf("\n");
    printf("============================================================\n");
    printf("  Matrix Module Performance Benchmarks\n");
    printf("  SIMD level: %s\n", fc_simd_level_string(fc_detect_simd()));
    printf("============================================================\n");

    run_gemm_benchmarks();
    run_gemv_benchmarks();
    run_transpose_benchmarks();
    run_vector_benchmarks();

    printf("\n");
    printf("============================================================\n");
    printf("  Benchmarks complete\n");
    printf("============================================================\n");
}
