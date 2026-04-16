/**
 * @file gemm_sse42.c
 * @brief SSE4.2 optimized implementation of GEMM
 *
 * Implements SSE4.2 SIMD instructions.
 * Uses 4x4 micro-kernel optimized for SSE registers (128-bit, 2 doubles).
 */

#include <string.h>
#include "../platform/platform.h"
#include "../platform/error.h"

#if FC_HAS_SSE42
#include <emmintrin.h>  /* SSE2 */
#include <smmintrin.h>  /* SSE4.2 */

/*
 * SSE4.2 optimized 4x4 micro-kernel
*/

/**
 * @brief 4x4 micro-kernel using SSE2 intrinsics
 *
 * Processes 4x4 block of C using SSE registers.
 * Each SSE register holds 2 doubles, so we process 2 columns at a time.
 */
static void gemm_micro_kernel_4x4_sse42(int k,
                                         const double* A, int lda,
                                         const double* B, int ldb,
                                         double* C, int ldc,
                                         double alpha, double beta) {
    /* Accumulator registers for C (4 rows x 2 columns per register) */
    __m128d c00_01 = _mm_setzero_pd();
    __m128d c02_03 = _mm_setzero_pd();
    __m128d c10_11 = _mm_setzero_pd();
    __m128d c12_13 = _mm_setzero_pd();
    __m128d c20_21 = _mm_setzero_pd();
    __m128d c22_23 = _mm_setzero_pd();
    __m128d c30_31 = _mm_setzero_pd();
    __m128d c32_33 = _mm_setzero_pd();

    /* Compute A * B */
    for (int p = 0; p < k; p++) {
        /* Load A column (4 elements) */
        double a0 = A[0 * lda + p];
        double a1 = A[1 * lda + p];
        double a2 = A[2 * lda + p];
        double a3 = A[3 * lda + p];

        /* Broadcast each A element to SSE register */
        __m128d a0_vec = _mm_set1_pd(a0);
        __m128d a1_vec = _mm_set1_pd(a1);
        __m128d a2_vec = _mm_set1_pd(a2);
        __m128d a3_vec = _mm_set1_pd(a3);

        /* Load B row (4 elements, 2 at a time) */
        __m128d b01 = _mm_loadu_pd(&B[p * ldb + 0]);
        __m128d b23 = _mm_loadu_pd(&B[p * ldb + 2]);

        /* Multiply and accumulate: C += A * B */
        c00_01 = _mm_add_pd(c00_01, _mm_mul_pd(a0_vec, b01));
        c02_03 = _mm_add_pd(c02_03, _mm_mul_pd(a0_vec, b23));

        c10_11 = _mm_add_pd(c10_11, _mm_mul_pd(a1_vec, b01));
        c12_13 = _mm_add_pd(c12_13, _mm_mul_pd(a1_vec, b23));

        c20_21 = _mm_add_pd(c20_21, _mm_mul_pd(a2_vec, b01));
        c22_23 = _mm_add_pd(c22_23, _mm_mul_pd(a2_vec, b23));

        c30_31 = _mm_add_pd(c30_31, _mm_mul_pd(a3_vec, b01));
        c32_33 = _mm_add_pd(c32_33, _mm_mul_pd(a3_vec, b23));
    }

    /* Scale by alpha */
    __m128d alpha_vec = _mm_set1_pd(alpha);
    c00_01 = _mm_mul_pd(c00_01, alpha_vec);
    c02_03 = _mm_mul_pd(c02_03, alpha_vec);
    c10_11 = _mm_mul_pd(c10_11, alpha_vec);
    c12_13 = _mm_mul_pd(c12_13, alpha_vec);
    c20_21 = _mm_mul_pd(c20_21, alpha_vec);
    c22_23 = _mm_mul_pd(c22_23, alpha_vec);
    c30_31 = _mm_mul_pd(c30_31, alpha_vec);
    c32_33 = _mm_mul_pd(c32_33, alpha_vec);

    /* Update C with beta * C + alpha * A*B */
    if (beta == 0.0) {
        _mm_storeu_pd(&C[0 * ldc + 0], c00_01);
        _mm_storeu_pd(&C[0 * ldc + 2], c02_03);
        _mm_storeu_pd(&C[1 * ldc + 0], c10_11);
        _mm_storeu_pd(&C[1 * ldc + 2], c12_13);
        _mm_storeu_pd(&C[2 * ldc + 0], c20_21);
        _mm_storeu_pd(&C[2 * ldc + 2], c22_23);
        _mm_storeu_pd(&C[3 * ldc + 0], c30_31);
        _mm_storeu_pd(&C[3 * ldc + 2], c32_33);
    } else {
        __m128d beta_vec = _mm_set1_pd(beta);

        __m128d c_old;
        c_old = _mm_loadu_pd(&C[0 * ldc + 0]);
        c_old = _mm_add_pd(_mm_mul_pd(c_old, beta_vec), c00_01);
        _mm_storeu_pd(&C[0 * ldc + 0], c_old);

        c_old = _mm_loadu_pd(&C[0 * ldc + 2]);
        c_old = _mm_add_pd(_mm_mul_pd(c_old, beta_vec), c02_03);
        _mm_storeu_pd(&C[0 * ldc + 2], c_old);

        c_old = _mm_loadu_pd(&C[1 * ldc + 0]);
        c_old = _mm_add_pd(_mm_mul_pd(c_old, beta_vec), c10_11);
        _mm_storeu_pd(&C[1 * ldc + 0], c_old);

        c_old = _mm_loadu_pd(&C[1 * ldc + 2]);
        c_old = _mm_add_pd(_mm_mul_pd(c_old, beta_vec), c12_13);
        _mm_storeu_pd(&C[1 * ldc + 2], c_old);

        c_old = _mm_loadu_pd(&C[2 * ldc + 0]);
        c_old = _mm_add_pd(_mm_mul_pd(c_old, beta_vec), c20_21);
        _mm_storeu_pd(&C[2 * ldc + 0], c_old);

        c_old = _mm_loadu_pd(&C[2 * ldc + 2]);
        c_old = _mm_add_pd(_mm_mul_pd(c_old, beta_vec), c22_23);
        _mm_storeu_pd(&C[2 * ldc + 2], c_old);

        c_old = _mm_loadu_pd(&C[3 * ldc + 0]);
        c_old = _mm_add_pd(_mm_mul_pd(c_old, beta_vec), c30_31);
        _mm_storeu_pd(&C[3 * ldc + 0], c_old);

        c_old = _mm_loadu_pd(&C[3 * ldc + 2]);
        c_old = _mm_add_pd(_mm_mul_pd(c_old, beta_vec), c32_33);
        _mm_storeu_pd(&C[3 * ldc + 2], c_old);
    }
}

/*
 * Edge case handlers
*/

static void gemm_edge_sse42(int m, int n, int k,
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
 * Main SSE4.2 GEMM implementation
*/

int fc_mat_gemm_f64_sse42(int m, int n, int k,
                          double alpha,
                          const double* A, int lda,
                          const double* B, int ldb,
                          double beta,
                          double* C, int ldc) {
    /* Input validation */
    if (m <= 0 || n <= 0 || k <= 0) {
        return FC_ERR_INVALID_ARG;
    }
    if (A == NULL || B == NULL || C == NULL) {
        return FC_ERR_INVALID_ARG;
    }
    if (lda < k || ldb < n || ldc < n) {
        return FC_ERR_INVALID_ARG;
    }

    /* Process in 4x4 blocks */
    const int MR = 4;
    const int NR = 4;
    int m_blocks = m / MR;
    int n_blocks = n / NR;
    int m_remainder = m % MR;
    int n_remainder = n % NR;

    /* Main 4x4 blocked region */
    for (int i = 0; i < m_blocks; i++) {
        for (int j = 0; j < n_blocks; j++) {
            gemm_micro_kernel_4x4_sse42(k,
                                        A + i * MR * lda,
                                        lda,
                                        B + j * NR,
                                        ldb,
                                        C + i * MR * ldc + j * NR,
                                        ldc,
                                        alpha, beta);
        }

        /* Right edge */
        if (n_remainder > 0) {
            gemm_edge_sse42(MR, n_remainder, k,
                            A + i * MR * lda,
                            lda,
                            B + n_blocks * NR,
                            ldb,
                            C + i * MR * ldc + n_blocks * NR,
                            ldc,
                            alpha, beta);
        }
    }

    /* Bottom edge */
    if (m_remainder > 0) {
        gemm_edge_sse42(m_remainder, n, k,
                        A + m_blocks * MR * lda,
                        lda,
                        B,
                        ldb,
                        C + m_blocks * MR * ldc,
                        ldc,
                        alpha, beta);
    }

    return FC_OK;
}

#else

/* Fallback when SSE4.2 is not available */
int fc_mat_gemm_f64_sse42(int m, int n, int k,
                          double alpha,
                          const double* A, int lda,
                          const double* B, int ldb,
                          double beta,
                          double* C, int ldc) {
    (void)m; (void)n; (void)k;
    (void)alpha; (void)A; (void)lda;
    (void)B; (void)ldb; (void)beta;
    (void)C; (void)ldc;
    return FC_ERR_NOT_IMPLEMENTED;
}

#endif /* FC_HAS_SSE42 */
