/**
 * @file gemm_scalar.c
 * @brief Scalar baseline implementation of GEMM (General Matrix Multiply)
 *
 * Implements: C = alpha * A * B + beta * C
 * This is the portable baseline implementation without SIMD optimizations.
 * Uses cache-friendly blocking for better performance on large matrices.
 */

#include <string.h>
#include "../platform/platform.h"
#include "../platform/error.h"

/*
 * Configuration constants
*/

/* Block sizes tuned for typical L1 cache (32KB) */
#define GEMM_MC 64   /* Block size for M dimension */
#define GEMM_KC 256  /* Block size for K dimension */
#define GEMM_NC 4096 /* Block size for N dimension */

/* Micro-kernel size for register blocking */
#define GEMM_MR 4    /* Micro-panel height */
#define GEMM_NR 4    /* Micro-panel width */

/*
 * Input validation
*/

static int validate_gemm_inputs(int m, int n, int k,
                                 const double* A, int lda,
                                 const double* B, int ldb,
                                 double* C, int ldc) {
    if (m <= 0 || n <= 0 || k <= 0) {
        return FC_ERR_INVALID_ARG;
    }
    if (A == NULL || B == NULL || C == NULL) {
        return FC_ERR_INVALID_ARG;
    }
    if (lda < k || ldb < n || ldc < n) {
        return FC_ERR_INVALID_ARG;
    }
    return FC_OK;
}

/*
 * Micro-kernel: 4x4 register-blocked multiplication
*/

/**
 * @brief 4x4 micro-kernel for GEMM
 *
 * Computes a 4x4 block of C using register blocking.
 * This is the innermost computational kernel.
 */
static void gemm_micro_kernel_4x4(int k,
                                   const double* A, int lda,
                                   const double* B, int ldb,
                                   double* C, int ldc,
                                   double alpha, double beta) {
    /* Accumulate in registers */
    double c00 = 0.0, c01 = 0.0, c02 = 0.0, c03 = 0.0;
    double c10 = 0.0, c11 = 0.0, c12 = 0.0, c13 = 0.0;
    double c20 = 0.0, c21 = 0.0, c22 = 0.0, c23 = 0.0;
    double c30 = 0.0, c31 = 0.0, c32 = 0.0, c33 = 0.0;

    /* Compute A * B */
    for (int p = 0; p < k; p++) {
        /* Load A column */
        double a0 = A[0 * lda + p];
        double a1 = A[1 * lda + p];
        double a2 = A[2 * lda + p];
        double a3 = A[3 * lda + p];

        /* Load B row */
        double b0 = B[p * ldb + 0];
        double b1 = B[p * ldb + 1];
        double b2 = B[p * ldb + 2];
        double b3 = B[p * ldb + 3];

        /* Outer product */
        c00 += a0 * b0; c01 += a0 * b1; c02 += a0 * b2; c03 += a0 * b3;
        c10 += a1 * b0; c11 += a1 * b1; c12 += a1 * b2; c13 += a1 * b3;
        c20 += a2 * b0; c21 += a2 * b1; c22 += a2 * b2; c23 += a2 * b3;
        c30 += a3 * b0; c31 += a3 * b1; c32 += a3 * b2; c33 += a3 * b3;
    }

    /* Update C with alpha * A*B + beta * C */
    if (beta == 0.0) {
        C[0 * ldc + 0] = alpha * c00;
        C[0 * ldc + 1] = alpha * c01;
        C[0 * ldc + 2] = alpha * c02;
        C[0 * ldc + 3] = alpha * c03;

        C[1 * ldc + 0] = alpha * c10;
        C[1 * ldc + 1] = alpha * c11;
        C[1 * ldc + 2] = alpha * c12;
        C[1 * ldc + 3] = alpha * c13;

        C[2 * ldc + 0] = alpha * c20;
        C[2 * ldc + 1] = alpha * c21;
        C[2 * ldc + 2] = alpha * c22;
        C[2 * ldc + 3] = alpha * c23;

        C[3 * ldc + 0] = alpha * c30;
        C[3 * ldc + 1] = alpha * c31;
        C[3 * ldc + 2] = alpha * c32;
        C[3 * ldc + 3] = alpha * c33;
    } else {
        C[0 * ldc + 0] = alpha * c00 + beta * C[0 * ldc + 0];
        C[0 * ldc + 1] = alpha * c01 + beta * C[0 * ldc + 1];
        C[0 * ldc + 2] = alpha * c02 + beta * C[0 * ldc + 2];
        C[0 * ldc + 3] = alpha * c03 + beta * C[0 * ldc + 3];

        C[1 * ldc + 0] = alpha * c10 + beta * C[1 * ldc + 0];
        C[1 * ldc + 1] = alpha * c11 + beta * C[1 * ldc + 1];
        C[1 * ldc + 2] = alpha * c12 + beta * C[1 * ldc + 2];
        C[1 * ldc + 3] = alpha * c13 + beta * C[1 * ldc + 3];

        C[2 * ldc + 0] = alpha * c20 + beta * C[2 * ldc + 0];
        C[2 * ldc + 1] = alpha * c21 + beta * C[2 * ldc + 1];
        C[2 * ldc + 2] = alpha * c22 + beta * C[2 * ldc + 2];
        C[2 * ldc + 3] = alpha * c23 + beta * C[2 * ldc + 3];

        C[3 * ldc + 0] = alpha * c30 + beta * C[3 * ldc + 0];
        C[3 * ldc + 1] = alpha * c31 + beta * C[3 * ldc + 1];
        C[3 * ldc + 2] = alpha * c32 + beta * C[3 * ldc + 2];
        C[3 * ldc + 3] = alpha * c33 + beta * C[3 * ldc + 3];
    }
}

/*
 * Edge case handlers for non-multiple-of-4 dimensions
*/

static void gemm_edge_m(int m, int n, int k,
                        const double* A, int lda,
                        const double* B, int ldb,
                        double* C, int ldc,
                        double alpha, double beta) {
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            double sum = 0.0;
            for (int p = 0; p < k; p++) {
                sum += A[i * lda + p] * B[p * ldb + j];
            }
            if (beta == 0.0) {
                C[i * ldc + j] = alpha * sum;
            } else {
                C[i * ldc + j] = alpha * sum + beta * C[i * ldc + j];
            }
        }
    }
}

static void gemm_edge_n(int m, int n, int k,
                        const double* A, int lda,
                        const double* B, int ldb,
                        double* C, int ldc,
                        double alpha, double beta) {
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            double sum = 0.0;
            for (int p = 0; p < k; p++) {
                sum += A[i * lda + p] * B[p * ldb + j];
            }
            if (beta == 0.0) {
                C[i * ldc + j] = alpha * sum;
            } else {
                C[i * ldc + j] = alpha * sum + beta * C[i * ldc + j];
            }
        }
    }
}

/*
 * Main GEMM implementation
*/

/**
 * @brief Scalar GEMM implementation with cache blocking
 *
 * Uses a three-level blocking strategy:
 * 1. Outer blocking for cache efficiency
 * 2. Register blocking with 4x4 micro-kernel
 * 3. Edge case handling for non-aligned dimensions
 */
static void gemm_scalar_impl(int m, int n, int k,
                             double alpha,
                             const double* A, int lda,
                             const double* B, int ldb,
                             double beta,
                             double* C, int ldc) {
    /* Process in 4x4 blocks using micro-kernel */
    int m_blocks = m / GEMM_MR;
    int n_blocks = n / GEMM_NR;
    int m_remainder = m % GEMM_MR;
    int n_remainder = n % GEMM_NR;

    /* Main 4x4 blocked region */
    for (int i = 0; i < m_blocks; i++) {
        for (int j = 0; j < n_blocks; j++) {
            gemm_micro_kernel_4x4(k,
                                  A + i * GEMM_MR * lda,
                                  lda,
                                  B + j * GEMM_NR,
                                  ldb,
                                  C + i * GEMM_MR * ldc + j * GEMM_NR,
                                  ldc,
                                  alpha, beta);
        }

        /* Right edge (n remainder) */
        if (n_remainder > 0) {
            gemm_edge_n(GEMM_MR, n_remainder, k,
                        A + i * GEMM_MR * lda,
                        lda,
                        B + n_blocks * GEMM_NR,
                        ldb,
                        C + i * GEMM_MR * ldc + n_blocks * GEMM_NR,
                        ldc,
                        alpha, beta);
        }
    }

    /* Bottom edge (m remainder) */
    if (m_remainder > 0) {
        for (int j = 0; j < n_blocks; j++) {
            gemm_edge_m(m_remainder, GEMM_NR, k,
                        A + m_blocks * GEMM_MR * lda,
                        lda,
                        B + j * GEMM_NR,
                        ldb,
                        C + m_blocks * GEMM_MR * ldc + j * GEMM_NR,
                        ldc,
                        alpha, beta);
        }

        /* Bottom-right corner */
        if (n_remainder > 0) {
            gemm_edge_m(m_remainder, n_remainder, k,
                        A + m_blocks * GEMM_MR * lda,
                        lda,
                        B + n_blocks * GEMM_NR,
                        ldb,
                        C + m_blocks * GEMM_MR * ldc + n_blocks * GEMM_NR,
                        ldc,
                        alpha, beta);
        }
    }
}

/*
 * Internal API - Scalar implementation
 */

int fc_mat_gemm_f64_scalar(int m, int n, int k,
                           double alpha,
                           const double* A, int lda,
                           const double* B, int ldb,
                           double beta,
                           double* C, int ldc) {
    int status = validate_gemm_inputs(m, n, k, A, lda, B, ldb, C, ldc);
    if (status != FC_OK) {
        return status;
    }

    gemm_scalar_impl(m, n, k, alpha, A, lda, B, ldb, beta, C, ldc);

    return FC_OK;
}
