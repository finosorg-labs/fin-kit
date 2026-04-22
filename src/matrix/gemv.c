/**
 * @file gemv.c
 * @brief General Matrix-Vector Multiply (GEMV) implementation
 *
 * Implements: y = alpha * A * x + beta * y
 * where A is m×n, x is n×1, y is m×1
 *
 * Provides SIMD-optimized implementations with cache blocking for different CPU capabilities.
 *
 * Cache blocking strategy:
 * - KC (column block): 512 elements (~4KB, fits in L1 cache)
 * - MC (row block): 64 rows (MC×KC block ~32KB, fits in L2 cache)
 */

#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <platform/platform.h>
#include <platform/error.h>
#include <platform/simd_detect.h>
#include <matrix/matrix_internal.h>

/* Cache blocking parameters */
#define GEMV_KC 512  /* Column block size (L1 cache) */
#define GEMV_MC 64   /* Row block size (L2 cache) */

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
    /* Apply beta scaling first if needed */
    if (beta == 0.0) {
        for (int i = 0; i < m; i++) {
            y[i] = 0.0;
        }
    } else if (beta != 1.0) {
        for (int i = 0; i < m; i++) {
            y[i] *= beta;
        }
    }

    /* Cache-blocked computation: outer loop over column blocks */
    for (int jc = 0; jc < n; jc += GEMV_KC) {
        int nc = (jc + GEMV_KC <= n) ? GEMV_KC : (n - jc);

        /* Inner loop over row blocks */
        for (int ic = 0; ic < m; ic += GEMV_MC) {
            int mc = (ic + GEMV_MC <= m) ? GEMV_MC : (m - ic);

            /* Process MC×KC block */
            for (int i = ic; i < ic + mc; i++) {
                __m256d sum_vec = _mm256_setzero_pd();
                int j = jc;

                /* Check alignment for this row */
                int is_aligned = (((uintptr_t)&A[i * lda + jc] & 31) == 0);

                /* SIMD loop */
                for (; j + 3 < jc + nc; j += 4) {
                    __m256d a_vec, x_vec;
                    if (is_aligned && ((uintptr_t)&x[j] & 31) == 0) {
                        a_vec = _mm256_load_pd(&A[i * lda + j]);
                        x_vec = _mm256_load_pd(&x[j]);
                    } else {
                        a_vec = _mm256_loadu_pd(&A[i * lda + j]);
                        x_vec = _mm256_loadu_pd(&x[j]);
                    }
                    sum_vec = _mm256_fmadd_pd(a_vec, x_vec, sum_vec);
                }

                /* Horizontal reduction */
                __m128d lo = _mm256_castpd256_pd128(sum_vec);
                __m128d hi = _mm256_extractf128_pd(sum_vec, 1);
                __m128d pair = _mm_add_pd(lo, hi);
                double sum = _mm_cvtsd_f64(pair) +
                            _mm_cvtsd_f64(_mm_unpackhi_pd(pair, pair));

                /* Scalar tail */
                for (; j < jc + nc; j++) {
                    sum += A[i * lda + j] * x[j];
                }

                /* Accumulate to output */
                y[i] += alpha * sum;
            }
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
    /* Apply beta scaling first if needed */
    if (beta == 0.0) {
        for (int i = 0; i < m; i++) {
            y[i] = 0.0;
        }
    } else if (beta != 1.0) {
        for (int i = 0; i < m; i++) {
            y[i] *= beta;
        }
    }

    /* Cache-blocked computation: outer loop over column blocks */
    for (int jc = 0; jc < n; jc += GEMV_KC) {
        int nc = (jc + GEMV_KC <= n) ? GEMV_KC : (n - jc);

        /* Inner loop over row blocks */
        for (int ic = 0; ic < m; ic += GEMV_MC) {
            int mc = (ic + GEMV_MC <= m) ? GEMV_MC : (m - ic);

            /* Process MC×KC block */
            for (int i = ic; i < ic + mc; i++) {
                __m128d sum_vec = _mm_setzero_pd();
                int j = jc;

                /* Check alignment for this row */
                int is_aligned = (((uintptr_t)&A[i * lda + jc] & 15) == 0);

                /* SIMD loop */
                for (; j + 1 < jc + nc; j += 2) {
                    __m128d a_vec, x_vec;
                    if (is_aligned && ((uintptr_t)&x[j] & 15) == 0) {
                        a_vec = _mm_load_pd(&A[i * lda + j]);
                        x_vec = _mm_load_pd(&x[j]);
                    } else {
                        a_vec = _mm_loadu_pd(&A[i * lda + j]);
                        x_vec = _mm_loadu_pd(&x[j]);
                    }
                    sum_vec = _mm_add_pd(sum_vec, _mm_mul_pd(a_vec, x_vec));
                }

                /* Horizontal reduction */
                double sum = _mm_cvtsd_f64(sum_vec) +
                            _mm_cvtsd_f64(_mm_unpackhi_pd(sum_vec, sum_vec));

                /* Scalar tail */
                for (; j < jc + nc; j++) {
                    sum += A[i * lda + j] * x[j];
                }

                /* Accumulate to output */
                y[i] += alpha * sum;
            }
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
    /* Apply beta scaling first if needed */
    if (beta == 0.0) {
        for (int i = 0; i < m; i++) {
            y[i] = 0.0;
        }
    } else if (beta != 1.0) {
        for (int i = 0; i < m; i++) {
            y[i] *= beta;
        }
    }

    /* Cache-blocked computation: outer loop over column blocks */
    for (int jc = 0; jc < n; jc += GEMV_KC) {
        int nc = (jc + GEMV_KC <= n) ? GEMV_KC : (n - jc);

        /* Inner loop over row blocks */
        for (int ic = 0; ic < m; ic += GEMV_MC) {
            int mc = (ic + GEMV_MC <= m) ? GEMV_MC : (m - ic);

            /* Process MC×KC block */
            for (int i = ic; i < ic + mc; i++) {
                double sum = 0.0;
                for (int j = jc; j < jc + nc; j++) {
                    sum += A[i * lda + j] * x[j];
                }
                /* Accumulate to output */
                y[i] += alpha * sum;
            }
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
