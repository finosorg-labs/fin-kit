/**
 * @file gemv.c
 * @brief General Matrix-Vector Multiply (GEMV) implementation
 *
 * Implements: y = alpha * A * x + beta * y
 * where A is m×n, x is n×1, y is m×1
 *
 * Provides SIMD-optimized implementations for different CPU capabilities.
 */

#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <fin-kit/platform/platform.h>
#include <fin-kit/platform/error.h>
#include <fin-kit/platform/simd_detect.h>
#include <fin-kit/matrix/matrix_internal.h>

#if FC_HAS_AVX2
#include <immintrin.h>

#ifdef FC_ENABLE_INTERNAL_TESTS
void
#else
static void
#endif
fc_mat_gemv_f64_avx2(int m, int n,
                     double alpha,
                     const double* A, int lda,
                     const double* x,
                     double beta,
                     double* y) {
    for (int i = 0; i < m; i++) {
        __m256d sum_vec = _mm256_setzero_pd();
        int j = 0;

        for (; j + 3 < n; j += 4) {
            __m256d a_vec = _mm256_loadu_pd(&A[i * lda + j]);
            __m256d x_vec = _mm256_loadu_pd(&x[j]);
            sum_vec = _mm256_fmadd_pd(a_vec, x_vec, sum_vec);
        }

        /* Pairwise horizontal reduction for better precision */
        __m128d lo = _mm256_castpd256_pd128(sum_vec);
        __m128d hi = _mm256_extractf128_pd(sum_vec, 1);
        __m128d pair = _mm_add_pd(lo, hi);
        double sum = _mm_cvtsd_f64(pair) +
                     _mm_cvtsd_f64(_mm_unpackhi_pd(pair, pair));

        /* Scalar tail with Kahan compensation */
        double c = 0.0;
        for (; j < n; j++) {
            double product = A[i * lda + j] * x[j];
            double t = product - c;
            double s = sum + t;
            c = (s - sum) - t;
            sum = s;
        }

        if (beta == 0.0) {
            y[i] = alpha * sum;
        } else {
            y[i] = alpha * sum + beta * y[i];
        }
    }
}

#endif

#if FC_HAS_SSE42
#include <emmintrin.h>
#include <smmintrin.h>

#ifdef FC_ENABLE_INTERNAL_TESTS
void
#else
static void
#endif
fc_mat_gemv_f64_sse42(int m, int n,
                      double alpha,
                      const double* A, int lda,
                      const double* x,
                      double beta,
                      double* y) {
    for (int i = 0; i < m; i++) {
        __m128d sum_vec = _mm_setzero_pd();
        int j = 0;

        for (; j + 1 < n; j += 2) {
            __m128d a_vec = _mm_loadu_pd(&A[i * lda + j]);
            __m128d x_vec = _mm_loadu_pd(&x[j]);
            sum_vec = _mm_add_pd(sum_vec, _mm_mul_pd(a_vec, x_vec));
        }

        double sum = _mm_cvtsd_f64(sum_vec) +
                     _mm_cvtsd_f64(_mm_unpackhi_pd(sum_vec, sum_vec));

        /* Scalar tail with Kahan compensation */
        double c = 0.0;
        for (; j < n; j++) {
            double product = A[i * lda + j] * x[j];
            double t = product - c;
            double s = sum + t;
            c = (s - sum) - t;
            sum = s;
        }

        if (beta == 0.0) {
            y[i] = alpha * sum;
        } else {
            y[i] = alpha * sum + beta * y[i];
        }
    }
}

#endif

#ifdef FC_ENABLE_INTERNAL_TESTS
void
#else
static void
#endif
fc_mat_gemv_f64_scalar(int m, int n,
                       double alpha,
                       const double* A, int lda,
                       const double* x,
                       double beta,
                       double* y) {
    for (int i = 0; i < m; i++) {
        /* Kahan summation for numerical stability */
        double sum = 0.0;
        double c = 0.0;
        for (int j = 0; j < n; j++) {
            double product = A[i * lda + j] * x[j];
            double t = product - c;
            double s = sum + t;
            c = (s - sum) - t;
            sum = s;
        }

        if (beta == 0.0) {
            y[i] = alpha * sum;
        } else {
            y[i] = alpha * sum + beta * y[i];
        }
    }
}

int fc_mat_gemv_f64(int64_t m, int64_t n,
                    double alpha,
                    const double* A, int64_t lda,
                    const double* x,
                    double beta,
                    double* y) {
    if (m <= 0 || n <= 0) {
        return FC_ERR_INVALID_ARG;
    }
    if (A == NULL || x == NULL || y == NULL) {
        return FC_ERR_INVALID_ARG;
    }
    if (lda < n) {
        return FC_ERR_INVALID_ARG;
    }
    if (m > INT_MAX || n > INT_MAX || lda > INT_MAX) {
        return FC_ERR_INVALID_ARG;
    }

    int m_int = (int)m;
    int n_int = (int)n;
    int lda_int = (int)lda;

#if FC_HAS_AVX2 || FC_HAS_SSE42
    fc_simd_level_t level = fc_get_simd_level();
#endif

#if FC_HAS_AVX2
    if (level >= FC_SIMD_AVX2) {
        fc_mat_gemv_f64_avx2(m_int, n_int, alpha, A, lda_int, x, beta, y);
        return FC_OK;
    }
#endif

#if FC_HAS_SSE42
    if (level >= FC_SIMD_SSE42) {
        fc_mat_gemv_f64_sse42(m_int, n_int, alpha, A, lda_int, x, beta, y);
        return FC_OK;
    }
#endif

    fc_mat_gemv_f64_scalar(m_int, n_int, alpha, A, lda_int, x, beta, y);
    return FC_OK;
}
