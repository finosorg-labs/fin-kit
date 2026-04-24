/**
 * @file gemm_avx512.c
 * @brief AVX-512 optimized implementation of GEMM
 *
 * Implements AVX-512 SIMD instructions for maximum performance.
 * Uses 4x8 micro-kernel optimized for AVX-512 registers (512-bit, 8 doubles).
 * Includes software prefetching to reduce cache miss latency.
 */

#include <string.h>
#include <platform.h>
#include <error.h>

#if FC_HAS_AVX512
#include <immintrin.h>

/* Prefetch distance (iterations ahead to prefetch) */
#define PREFETCH_DISTANCE 8

/**
 * @brief 4x8 micro-kernel using AVX-512 intrinsics with prefetching
 *
 * Processes 4x8 block of C using AVX-512 registers.
 * Each AVX-512 register holds 8 doubles, processing full row at once.
 * Prefetches A and B data ahead to hide memory latency.
 */
static void gemm_micro_kernel_4x8_avx512(int k,
                                          const double* A, int lda,
                                          const double* B, int ldb,
                                          double* C, int ldc,
                                          double alpha, double beta) {
    __m512d c0 = _mm512_setzero_pd();
    __m512d c1 = _mm512_setzero_pd();
    __m512d c2 = _mm512_setzero_pd();
    __m512d c3 = _mm512_setzero_pd();

    for (int p = 0; p < k; p++) {
        /* Prefetch A and B data ahead */
        if (p + PREFETCH_DISTANCE < k) {
            _mm_prefetch((const char*)&A[0 * lda + p + PREFETCH_DISTANCE], _MM_HINT_T0);
            _mm_prefetch((const char*)&A[1 * lda + p + PREFETCH_DISTANCE], _MM_HINT_T0);
            _mm_prefetch((const char*)&A[2 * lda + p + PREFETCH_DISTANCE], _MM_HINT_T0);
            _mm_prefetch((const char*)&A[3 * lda + p + PREFETCH_DISTANCE], _MM_HINT_T0);
            _mm_prefetch((const char*)&B[(p + PREFETCH_DISTANCE) * ldb], _MM_HINT_T0);
        }

        double a0 = A[0 * lda + p];
        double a1 = A[1 * lda + p];
        double a2 = A[2 * lda + p];
        double a3 = A[3 * lda + p];

        __m512d a0_vec = _mm512_set1_pd(a0);
        __m512d a1_vec = _mm512_set1_pd(a1);
        __m512d a2_vec = _mm512_set1_pd(a2);
        __m512d a3_vec = _mm512_set1_pd(a3);

        /* Use aligned load if B row is 64-byte aligned */
        __m512d b;
        if (((uintptr_t)&B[p * ldb] & 63) == 0) {
            b = _mm512_load_pd(&B[p * ldb]);
        } else {
            b = _mm512_loadu_pd(&B[p * ldb]);
        }

        c0 = _mm512_fmadd_pd(a0_vec, b, c0);
        c1 = _mm512_fmadd_pd(a1_vec, b, c1);
        c2 = _mm512_fmadd_pd(a2_vec, b, c2);
        c3 = _mm512_fmadd_pd(a3_vec, b, c3);
    }

    __m512d alpha_vec = _mm512_set1_pd(alpha);
    c0 = _mm512_mul_pd(c0, alpha_vec);
    c1 = _mm512_mul_pd(c1, alpha_vec);
    c2 = _mm512_mul_pd(c2, alpha_vec);
    c3 = _mm512_mul_pd(c3, alpha_vec);

    if (beta == 0.0) {
        /* Use aligned stores if C is 64-byte aligned */
        if (((uintptr_t)C & 63) == 0 && (ldc * sizeof(double)) % 64 == 0) {
            _mm512_store_pd(&C[0 * ldc], c0);
            _mm512_store_pd(&C[1 * ldc], c1);
            _mm512_store_pd(&C[2 * ldc], c2);
            _mm512_store_pd(&C[3 * ldc], c3);
        } else {
            _mm512_storeu_pd(&C[0 * ldc], c0);
            _mm512_storeu_pd(&C[1 * ldc], c1);
            _mm512_storeu_pd(&C[2 * ldc], c2);
            _mm512_storeu_pd(&C[3 * ldc], c3);
        }
    } else {
        __m512d beta_vec = _mm512_set1_pd(beta);
        __m512d c_old;

        /* Use aligned loads/stores if C is 64-byte aligned */
        if (((uintptr_t)C & 63) == 0 && (ldc * sizeof(double)) % 64 == 0) {
            c_old = _mm512_load_pd(&C[0 * ldc]);
            c_old = _mm512_fmadd_pd(beta_vec, c_old, c0);
            _mm512_store_pd(&C[0 * ldc], c_old);

            c_old = _mm512_load_pd(&C[1 * ldc]);
            c_old = _mm512_fmadd_pd(beta_vec, c_old, c1);
            _mm512_store_pd(&C[1 * ldc], c_old);

            c_old = _mm512_load_pd(&C[2 * ldc]);
            c_old = _mm512_fmadd_pd(beta_vec, c_old, c2);
            _mm512_store_pd(&C[2 * ldc], c_old);

            c_old = _mm512_load_pd(&C[3 * ldc]);
            c_old = _mm512_fmadd_pd(beta_vec, c_old, c3);
            _mm512_store_pd(&C[3 * ldc], c_old);
        } else {
            c_old = _mm512_loadu_pd(&C[0 * ldc]);
            c_old = _mm512_fmadd_pd(beta_vec, c_old, c0);
            _mm512_storeu_pd(&C[0 * ldc], c_old);

            c_old = _mm512_loadu_pd(&C[1 * ldc]);
            c_old = _mm512_fmadd_pd(beta_vec, c_old, c1);
            _mm512_storeu_pd(&C[1 * ldc], c_old);

            c_old = _mm512_loadu_pd(&C[2 * ldc]);
            c_old = _mm512_fmadd_pd(beta_vec, c_old, c2);
            _mm512_storeu_pd(&C[2 * ldc], c_old);

            c_old = _mm512_loadu_pd(&C[3 * ldc]);
            c_old = _mm512_fmadd_pd(beta_vec, c_old, c3);
            _mm512_storeu_pd(&C[3 * ldc], c_old);
        }
    }
}

static void gemm_edge_avx512(int m, int n, int k,
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

int fc_mat_gemm_f64_avx512(int m, int n, int k,
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
            gemm_micro_kernel_4x8_avx512(k,
                                         A + i * MR * lda,
                                         lda,
                                         B + j * NR,
                                         ldb,
                                         C + i * MR * ldc + j * NR,
                                         ldc,
                                         alpha, beta);
        }

        if (n_remainder > 0) {
            gemm_edge_avx512(MR, n_remainder, k,
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
        gemm_edge_avx512(m_remainder, n, k,
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

int fc_mat_gemm_f64_avx512(int m, int n, int k,
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
