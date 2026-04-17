/**
 * @file gemm_avx2.c
 * @brief AVX2 optimized implementation of GEMM
 *
 * Implements AVX2 SIMD instructions for high-performance matrix multiplication.
 * Uses 4x8 micro-kernel optimized for AVX2 registers (256-bit, 4 doubles).
 * Includes software prefetching to reduce cache miss latency.
 */

#include <string.h>
#include <fin-kit/platform/platform.h>
#include <fin-kit/platform/error.h>

#if FC_HAS_AVX2
#include <immintrin.h>

/* Prefetch distance (iterations ahead to prefetch) */
#define PREFETCH_DISTANCE 8

/**
 * @brief 4x8 micro-kernel using AVX2 intrinsics with prefetching
 *
 * Processes 4x8 block of C using AVX2 registers.
 * Each AVX2 register holds 4 doubles, so we process 4 columns at a time.
 * Prefetches A and B data ahead to hide memory latency.
 */
static void gemm_micro_kernel_4x8_avx2(int k,
                                        const double* A, int lda,
                                        const double* B, int ldb,
                                        double* C, int ldc,
                                        double alpha, double beta) {
    __m256d c00_03 = _mm256_setzero_pd();
    __m256d c04_07 = _mm256_setzero_pd();
    __m256d c10_13 = _mm256_setzero_pd();
    __m256d c14_17 = _mm256_setzero_pd();
    __m256d c20_23 = _mm256_setzero_pd();
    __m256d c24_27 = _mm256_setzero_pd();
    __m256d c30_33 = _mm256_setzero_pd();
    __m256d c34_37 = _mm256_setzero_pd();

    for (int p = 0; p < k; p++) {
        /* Prefetch A and B data ahead */
        if (p + PREFETCH_DISTANCE < k) {
            _mm_prefetch((const char*)&A[0 * lda + p + PREFETCH_DISTANCE], _MM_HINT_T0);
            _mm_prefetch((const char*)&A[1 * lda + p + PREFETCH_DISTANCE], _MM_HINT_T0);
            _mm_prefetch((const char*)&A[2 * lda + p + PREFETCH_DISTANCE], _MM_HINT_T0);
            _mm_prefetch((const char*)&A[3 * lda + p + PREFETCH_DISTANCE], _MM_HINT_T0);
            _mm_prefetch((const char*)&B[(p + PREFETCH_DISTANCE) * ldb + 0], _MM_HINT_T0);
            _mm_prefetch((const char*)&B[(p + PREFETCH_DISTANCE) * ldb + 4], _MM_HINT_T0);
        }

        double a0 = A[0 * lda + p];
        double a1 = A[1 * lda + p];
        double a2 = A[2 * lda + p];
        double a3 = A[3 * lda + p];

        __m256d a0_vec = _mm256_set1_pd(a0);
        __m256d a1_vec = _mm256_set1_pd(a1);
        __m256d a2_vec = _mm256_set1_pd(a2);
        __m256d a3_vec = _mm256_set1_pd(a3);

        __m256d b03 = _mm256_loadu_pd(&B[p * ldb + 0]);
        __m256d b47 = _mm256_loadu_pd(&B[p * ldb + 4]);

        c00_03 = _mm256_fmadd_pd(a0_vec, b03, c00_03);
        c04_07 = _mm256_fmadd_pd(a0_vec, b47, c04_07);

        c10_13 = _mm256_fmadd_pd(a1_vec, b03, c10_13);
        c14_17 = _mm256_fmadd_pd(a1_vec, b47, c14_17);

        c20_23 = _mm256_fmadd_pd(a2_vec, b03, c20_23);
        c24_27 = _mm256_fmadd_pd(a2_vec, b47, c24_27);

        c30_33 = _mm256_fmadd_pd(a3_vec, b03, c30_33);
        c34_37 = _mm256_fmadd_pd(a3_vec, b47, c34_37);
    }

    __m256d alpha_vec = _mm256_set1_pd(alpha);
    c00_03 = _mm256_mul_pd(c00_03, alpha_vec);
    c04_07 = _mm256_mul_pd(c04_07, alpha_vec);
    c10_13 = _mm256_mul_pd(c10_13, alpha_vec);
    c14_17 = _mm256_mul_pd(c14_17, alpha_vec);
    c20_23 = _mm256_mul_pd(c20_23, alpha_vec);
    c24_27 = _mm256_mul_pd(c24_27, alpha_vec);
    c30_33 = _mm256_mul_pd(c30_33, alpha_vec);
    c34_37 = _mm256_mul_pd(c34_37, alpha_vec);

    if (beta == 0.0) {
        _mm256_storeu_pd(&C[0 * ldc + 0], c00_03);
        _mm256_storeu_pd(&C[0 * ldc + 4], c04_07);
        _mm256_storeu_pd(&C[1 * ldc + 0], c10_13);
        _mm256_storeu_pd(&C[1 * ldc + 4], c14_17);
        _mm256_storeu_pd(&C[2 * ldc + 0], c20_23);
        _mm256_storeu_pd(&C[2 * ldc + 4], c24_27);
        _mm256_storeu_pd(&C[3 * ldc + 0], c30_33);
        _mm256_storeu_pd(&C[3 * ldc + 4], c34_37);
    } else {
        __m256d beta_vec = _mm256_set1_pd(beta);
        __m256d c_old;

        c_old = _mm256_loadu_pd(&C[0 * ldc + 0]);
        c_old = _mm256_fmadd_pd(c_old, beta_vec, c00_03);
        _mm256_storeu_pd(&C[0 * ldc + 0], c_old);

        c_old = _mm256_loadu_pd(&C[0 * ldc + 4]);
        c_old = _mm256_fmadd_pd(c_old, beta_vec, c04_07);
        _mm256_storeu_pd(&C[0 * ldc + 4], c_old);

        c_old = _mm256_loadu_pd(&C[1 * ldc + 0]);
        c_old = _mm256_fmadd_pd(c_old, beta_vec, c10_13);
        _mm256_storeu_pd(&C[1 * ldc + 0], c_old);

        c_old = _mm256_loadu_pd(&C[1 * ldc + 4]);
        c_old = _mm256_fmadd_pd(c_old, beta_vec, c14_17);
        _mm256_storeu_pd(&C[1 * ldc + 4], c_old);

        c_old = _mm256_loadu_pd(&C[2 * ldc + 0]);
        c_old = _mm256_fmadd_pd(c_old, beta_vec, c20_23);
        _mm256_storeu_pd(&C[2 * ldc + 0], c_old);

        c_old = _mm256_loadu_pd(&C[2 * ldc + 4]);
        c_old = _mm256_fmadd_pd(c_old, beta_vec, c24_27);
        _mm256_storeu_pd(&C[2 * ldc + 4], c_old);

        c_old = _mm256_loadu_pd(&C[3 * ldc + 0]);
        c_old = _mm256_fmadd_pd(c_old, beta_vec, c30_33);
        _mm256_storeu_pd(&C[3 * ldc + 0], c_old);

        c_old = _mm256_loadu_pd(&C[3 * ldc + 4]);
        c_old = _mm256_fmadd_pd(c_old, beta_vec, c34_37);
        _mm256_storeu_pd(&C[3 * ldc + 4], c_old);
    }
}

static void gemm_edge_avx2(int m, int n, int k,
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

int fc_mat_gemm_f64_avx2(int m, int n, int k,
                         double alpha,
                         const double* A, int lda,
                         const double* B, int ldb,
                         double beta,
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

    const int MR = 4;
    const int NR = 8;
    int m_blocks = m / MR;
    int n_blocks = n / NR;
    int m_remainder = m % MR;
    int n_remainder = n % NR;

    for (int i = 0; i < m_blocks; i++) {
        for (int j = 0; j < n_blocks; j++) {
            gemm_micro_kernel_4x8_avx2(k,
                                       A + i * MR * lda,
                                       lda,
                                       B + j * NR,
                                       ldb,
                                       C + i * MR * ldc + j * NR,
                                       ldc,
                                       alpha, beta);
        }

        if (n_remainder > 0) {
            gemm_edge_avx2(MR, n_remainder, k,
                           A + i * MR * lda,
                           lda,
                           B + n_blocks * NR,
                           ldb,
                           C + i * MR * ldc + n_blocks * NR,
                           ldc,
                           alpha, beta);
        }
    }

    if (m_remainder > 0) {
        gemm_edge_avx2(m_remainder, n, k,
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

int fc_mat_gemm_f64_avx2(int m, int n, int k,
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

#endif
