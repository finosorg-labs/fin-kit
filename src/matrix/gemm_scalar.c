/**
 * @file gemm_scalar.c
 * @brief Scalar baseline implementation of GEMM (General Matrix Multiply)
 *
 * Implements: C = alpha * A * B + beta * C
 * This is the portable baseline implementation without SIMD optimizations.
 * Uses cache-friendly blocking for better performance on large matrices.
 */

#include <string.h>
#include <platform.h>
#include <error.h>

/*
 * Configuration constants
*/

/* Block sizes tuned for typical L1 cache (32KB) */
#define GEMM_MC 64   /* Block size for M dimension */
#define GEMM_KC 256  /* Block size for K dimension */
#define GEMM_NC 4096 /* Block size for N dimension */

/* Micro-kernel size for register blocking */
#define GEMM_MR 8    /* Micro-panel height */
#define GEMM_NR 8    /* Micro-panel width */

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
 * Micro-kernel: 8x8 register-blocked multiplication
*/

/**
 * @brief 8x8 micro-kernel for GEMM
 *
 * Computes an 8x8 block of C using register blocking.
 * This is the innermost computational kernel optimized for better register utilization.
 */
static void gemm_micro_kernel_8x8(int k,
                                   const double* A, int lda,
                                   const double* B, int ldb,
                                   double* C, int ldc,
                                   double alpha, double beta) {
    /* Accumulate in registers (64 accumulators for 8x8 block) */
    double c[8][8];
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            c[i][j] = 0.0;
        }
    }

    /* Compute A * B with software prefetching */
    for (int p = 0; p < k; p++) {
        /* Prefetch next iteration */
        if (p + 8 < k) {
            __builtin_prefetch(&A[0 * lda + p + 8], 0, 3);
            __builtin_prefetch(&B[(p + 8) * ldb], 0, 3);
        }

        /* Load A column (8 elements) */
        double a[8];
        for (int i = 0; i < 8; i++) {
            a[i] = A[i * lda + p];
        }

        /* Load B row (8 elements) */
        double b[8];
        for (int j = 0; j < 8; j++) {
            b[j] = B[p * ldb + j];
        }

        /* Outer product: unrolled for better instruction-level parallelism */
        for (int i = 0; i < 8; i++) {
            c[i][0] += a[i] * b[0];
            c[i][1] += a[i] * b[1];
            c[i][2] += a[i] * b[2];
            c[i][3] += a[i] * b[3];
            c[i][4] += a[i] * b[4];
            c[i][5] += a[i] * b[5];
            c[i][6] += a[i] * b[6];
            c[i][7] += a[i] * b[7];
        }
    }

    /* Update C with alpha * A*B + beta * C */
    if (beta == 0.0) {
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                C[i * ldc + j] = alpha * c[i][j];
            }
        }
    } else {
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                C[i * ldc + j] = alpha * c[i][j] + beta * C[i * ldc + j];
            }
        }
    }
}

/*
 * Edge case handler for non-multiple-of-4 dimensions
*/

static void gemm_edge(int m, int n, int k,
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
 * Panel computation with micro-kernel blocking
*/

/**
 * @brief Compute a panel of C using micro-kernel blocking
 *
 * Computes C_panel = alpha * A_panel * B_panel + beta * C_panel
 * where panels have dimensions: A (m_panel × k_panel), B (k_panel × n_panel)
 */
static void gemm_panel(int m_panel, int n_panel, int k_panel,
                       double alpha,
                       const double* A, int lda,
                       const double* B, int ldb,
                       double beta,
                       double* C, int ldc) {
    int m_blocks = m_panel / GEMM_MR;
    int n_blocks = n_panel / GEMM_NR;
    int m_remainder = m_panel % GEMM_MR;
    int n_remainder = n_panel % GEMM_NR;

    /* Main micro-kernel blocked region */
    for (int i = 0; i < m_blocks; i++) {
        for (int j = 0; j < n_blocks; j++) {
            gemm_micro_kernel_8x8(k_panel,
                                  A + i * GEMM_MR * lda,
                                  lda,
                                  B + j * GEMM_NR,
                                  ldb,
                                  C + i * GEMM_MR * ldc + j * GEMM_NR,
                                  ldc,
                                  alpha, beta);
        }

        /* Right edge */
        if (n_remainder > 0) {
            gemm_edge(GEMM_MR, n_remainder, k_panel,
                      A + i * GEMM_MR * lda,
                      lda,
                      B + n_blocks * GEMM_NR,
                      ldb,
                      C + i * GEMM_MR * ldc + n_blocks * GEMM_NR,
                      ldc,
                      alpha, beta);
        }
    }

    /* Bottom edge */
    if (m_remainder > 0) {
        for (int j = 0; j < n_blocks; j++) {
            gemm_edge(m_remainder, GEMM_NR, k_panel,
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
            gemm_edge(m_remainder, n_remainder, k_panel,
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
 * Main GEMM implementation with three-level cache blocking
*/

/**
 * @brief Scalar GEMM implementation with three-level cache blocking
 *
 * Blocking strategy (following BLIS/GotoBLAS approach):
 * 1. NC blocking: Keep B panel in L3 cache
 * 2. KC blocking: Keep B sub-panel in L2 cache
 * 3. MC blocking: Keep A sub-panel in L1 cache
 * 4. MR×NR micro-kernel: Register-level blocking
 *
 * Loop structure: for jc, pc, ic (N, K, M order)
 * This maximizes reuse of B data in cache.
 */
static void gemm_scalar_impl(int m, int n, int k,
                             double alpha,
                             const double* A, int lda,
                             const double* B, int ldb,
                             double beta,
                             double* C, int ldc) {
    /* Loop over N dimension (NC blocks) */
    for (int jc = 0; jc < n; jc += GEMM_NC) {
        int nc = (jc + GEMM_NC <= n) ? GEMM_NC : (n - jc);

        /* Loop over K dimension (KC blocks) */
        for (int pc = 0; pc < k; pc += GEMM_KC) {
            int kc = (pc + GEMM_KC <= k) ? GEMM_KC : (k - pc);

            /* Determine beta for this K iteration */
            double beta_panel = (pc == 0) ? beta : 1.0;

            /* Loop over M dimension (MC blocks) */
            for (int ic = 0; ic < m; ic += GEMM_MC) {
                int mc = (ic + GEMM_MC <= m) ? GEMM_MC : (m - ic);

                /* Compute panel: C[ic:ic+mc, jc:jc+nc] += A[ic:ic+mc, pc:pc+kc] * B[pc:pc+kc, jc:jc+nc] */
                gemm_panel(mc, nc, kc,
                          alpha,
                          A + ic * lda + pc,
                          lda,
                          B + pc * ldb + jc,
                          ldb,
                          beta_panel,
                          C + ic * ldc + jc,
                          ldc);
            }
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
