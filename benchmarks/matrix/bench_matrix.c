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

typedef struct {
    int n;
    double *A, *A_orig;
    int64_t *ipiv;
} lu_data_t;

typedef struct {
    int m, n;
    double *A, *A_orig, *tau;
} qr_data_t;

typedef struct {
    int n;
    double *A, *A_orig;
} cholesky_data_t;

typedef struct {
    int n, nrhs;
    double *A, *A_orig, *b, *b_orig;
    int64_t *ipiv;
} solve_data_t;

typedef struct {
    int n;
    double *A, *A_orig;
} inverse_data_t;

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

/* LU decomposition benchmark function */
static void bench_lu_fn(void* user_data) {
    lu_data_t* d = (lu_data_t*)user_data;
    /* Restore input data (in-place operation) */
    memcpy(d->A, d->A_orig, d->n * d->n * sizeof(double));
    fc_mat_lu_decompose_f64(d->n, d->A, d->n, d->ipiv);
}

/* QR decomposition benchmark function */
static void bench_qr_fn(void* user_data) {
    qr_data_t* d = (qr_data_t*)user_data;
    /* Restore input data (in-place operation) */
    memcpy(d->A, d->A_orig, d->m * d->n * sizeof(double));
    fc_mat_qr_decompose_f64(d->m, d->n, d->A, d->n, d->tau);
}

/* Cholesky decomposition benchmark function */
static void bench_cholesky_fn(void* user_data) {
    cholesky_data_t* d = (cholesky_data_t*)user_data;
    /* Restore input data (in-place operation) */
    memcpy(d->A, d->A_orig, d->n * d->n * sizeof(double));
    fc_mat_cholesky_decompose_f64(d->n, d->A, d->n);
}

/* Linear solve benchmark function */
static void bench_solve_fn(void* user_data) {
    solve_data_t* d = (solve_data_t*)user_data;
    /* Restore input data (in-place operation) */
    memcpy(d->A, d->A_orig, d->n * d->n * sizeof(double));
    memcpy(d->b, d->b_orig, d->n * sizeof(double));
    fc_mat_solve_linear_f64(d->n, 1, d->A, d->n, d->b, 1);
}

/* Matrix inverse benchmark function */
static void bench_inverse_fn(void* user_data) {
    inverse_data_t* d = (inverse_data_t*)user_data;
    /* Restore input data (in-place operation) */
    memcpy(d->A, d->A_orig, d->n * d->n * sizeof(double));
    fc_mat_inverse_f64(d->n, d->A, d->n);
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
        }

        free(data.x);
        free(data.y);
    }
}

/* Helper: create symmetric positive definite matrix */
static void make_spd_matrix(double* A, int n) {
    /* A = B^T * B + n*I ensures SPD */
    double* B = (double*)malloc(n * n * sizeof(double));
    fill_random(B, n * n);

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            double sum = 0.0;
            for (int k = 0; k < n; k++) {
                sum += B[k * n + i] * B[k * n + j];
            }
            A[i * n + j] = sum;
        }
        A[i * n + i] += n;  /* Add diagonal dominance */
    }

    free(B);
}

/* Run LU decomposition benchmarks */
static void run_lu_benchmarks(void) {
    printf("\n");
    printf("LU Decomposition Benchmarks (double precision)\n");
    printf("------------------------------------------------------------\n");

    int sizes[] = {16, 32, 64, 128, 256};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int s = 0; s < num_sizes; s++) {
        int n = sizes[s];
        lu_data_t data;
        data.n = n;
        data.A = (double*)malloc(n * n * sizeof(double));
        data.A_orig = (double*)malloc(n * n * sizeof(double));
        data.ipiv = (int64_t*)malloc(n * sizeof(int64_t));

        fill_random(data.A_orig, n * n);
        memcpy(data.A, data.A_orig, n * n * sizeof(double));

        char name[64];
        snprintf(name, sizeof(name), "LU %dx%d", n, n);

        fc_bench_config_t config = FC_BENCH_CONFIG_DEFAULT;
        config.name = name;
        config.data_size = n * n * sizeof(double);
        config.min_time_ms = 200.0;
        config.warmup_ms = 50.0;

        fc_bench_result_t result;
        fc_bench_run(&config, bench_lu_fn, &data, &result);

        free(data.A);
        free(data.A_orig);
        free(data.ipiv);
    }
}

/* Run QR decomposition benchmarks */
static void run_qr_benchmarks(void) {
    printf("\n");
    printf("QR Decomposition Benchmarks (double precision)\n");
    printf("------------------------------------------------------------\n");

    int sizes[] = {16, 32, 64, 128, 256};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int s = 0; s < num_sizes; s++) {
        int n = sizes[s];
        qr_data_t data;
        data.m = n;
        data.n = n;
        data.A = (double*)malloc(n * n * sizeof(double));
        data.A_orig = (double*)malloc(n * n * sizeof(double));
        data.tau = (double*)malloc(n * sizeof(double));

        fill_random(data.A_orig, n * n);
        memcpy(data.A, data.A_orig, n * n * sizeof(double));

        char name[64];
        snprintf(name, sizeof(name), "QR %dx%d", n, n);

        fc_bench_config_t config = FC_BENCH_CONFIG_DEFAULT;
        config.name = name;
        config.data_size = n * n * sizeof(double);
        config.min_time_ms = 200.0;
        config.warmup_ms = 50.0;

        fc_bench_result_t result;
        fc_bench_run(&config, bench_qr_fn, &data, &result);

        free(data.A);
        free(data.A_orig);
        free(data.tau);
    }
}

/* Run Cholesky decomposition benchmarks */
static void run_cholesky_benchmarks(void) {
    printf("\n");
    printf("Cholesky Decomposition Benchmarks (double precision)\n");
    printf("------------------------------------------------------------\n");

    int sizes[] = {16, 32, 64, 128, 256};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int s = 0; s < num_sizes; s++) {
        int n = sizes[s];
        cholesky_data_t data;
        data.n = n;
        data.A = (double*)malloc(n * n * sizeof(double));
        data.A_orig = (double*)malloc(n * n * sizeof(double));

        make_spd_matrix(data.A_orig, n);
        memcpy(data.A, data.A_orig, n * n * sizeof(double));

        char name[64];
        snprintf(name, sizeof(name), "Cholesky %dx%d", n, n);

        fc_bench_config_t config = FC_BENCH_CONFIG_DEFAULT;
        config.name = name;
        config.data_size = n * n * sizeof(double);
        config.min_time_ms = 200.0;
        config.warmup_ms = 50.0;

        fc_bench_result_t result;
        fc_bench_run(&config, bench_cholesky_fn, &data, &result);

        free(data.A);
        free(data.A_orig);
    }
}

/* Run linear solve benchmarks */
static void run_solve_benchmarks(void) {
    printf("\n");
    printf("Linear Solve Benchmarks (Ax=b, double precision)\n");
    printf("------------------------------------------------------------\n");

    int sizes[] = {16, 32, 64, 128, 256};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int s = 0; s < num_sizes; s++) {
        int n = sizes[s];
        solve_data_t data;
        data.n = n;
        data.nrhs = 1;
        data.A = (double*)malloc(n * n * sizeof(double));
        data.A_orig = (double*)malloc(n * n * sizeof(double));
        data.b = (double*)malloc(n * sizeof(double));
        data.b_orig = (double*)malloc(n * sizeof(double));
        data.ipiv = (int64_t*)malloc(n * sizeof(int64_t));

        fill_random(data.A_orig, n * n);
        fill_random(data.b_orig, n);
        memcpy(data.A, data.A_orig, n * n * sizeof(double));
        memcpy(data.b, data.b_orig, n * sizeof(double));

        char name[64];
        snprintf(name, sizeof(name), "Solve %dx%d", n, n);

        fc_bench_config_t config = FC_BENCH_CONFIG_DEFAULT;
        config.name = name;
        config.data_size = (n * n + n) * sizeof(double);
        config.min_time_ms = 200.0;
        config.warmup_ms = 50.0;

        fc_bench_result_t result;
        fc_bench_run(&config, bench_solve_fn, &data, &result);

        free(data.A);
        free(data.A_orig);
        free(data.b);
        free(data.b_orig);
        free(data.ipiv);
    }
}

/* Run matrix inverse benchmarks */
static void run_inverse_benchmarks(void) {
    printf("\n");
    printf("Matrix Inverse Benchmarks (double precision)\n");
    printf("------------------------------------------------------------\n");

    int sizes[] = {16, 32, 64, 128, 256};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int s = 0; s < num_sizes; s++) {
        int n = sizes[s];
        inverse_data_t data;
        data.n = n;
        data.A = (double*)malloc(n * n * sizeof(double));
        data.A_orig = (double*)malloc(n * n * sizeof(double));

        fill_random(data.A_orig, n * n);
        memcpy(data.A, data.A_orig, n * n * sizeof(double));

        char name[64];
        snprintf(name, sizeof(name), "Inverse %dx%d", n, n);

        fc_bench_config_t config = FC_BENCH_CONFIG_DEFAULT;
        config.name = name;
        config.data_size = n * n * sizeof(double);
        config.min_time_ms = 200.0;
        config.warmup_ms = 50.0;

        fc_bench_result_t result;
        fc_bench_run(&config, bench_inverse_fn, &data, &result);

        free(data.A);
        free(data.A_orig);
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
    run_lu_benchmarks();
    run_qr_benchmarks();
    run_cholesky_benchmarks();
    run_solve_benchmarks();
    run_inverse_benchmarks();

    printf("\n");
    printf("============================================================\n");
    printf("  Benchmarks complete\n");
    printf("============================================================\n");
}
