/**
 * @file vector_ops.c
 * @brief Vector operations implementation (dot product, norms, distances)
 *
 * Provides SIMD-optimized implementations with numerical stability.
 */

#include <math.h>
#include <float.h>
#include <stdint.h>
#include <limits.h>
#include <fin-kit/platform/platform.h>
#include <fin-kit/platform/error.h>
#include <fin-kit/platform/simd_detect.h>

#if FC_HAS_AVX2
#include <immintrin.h>
#endif

#if FC_HAS_SSE42
#include <emmintrin.h>
#include <smmintrin.h>
#endif

/*
 * Input validation helpers
*/

static FC_INLINE int validate_vector_inputs(const double* x, const double* y, int64_t n) {
    if (x == NULL || y == NULL) {
        return FC_ERR_INVALID_ARG;
    }
    if (n <= 0) {
        return FC_ERR_INVALID_ARG;
    }
    return FC_OK;
}

static FC_INLINE int validate_single_vector(const double* x, int64_t n) {
    if (x == NULL) {
        return FC_ERR_INVALID_ARG;
    }
    if (n <= 0) {
        return FC_ERR_INVALID_ARG;
    }
    return FC_OK;
}

/*
 * Dot product
*/

#if FC_HAS_AVX2
static inline double fc_vec_dot_f64_avx2(const double* x, const double* y, int64_t n) {
    __m256d sum_vec = _mm256_setzero_pd();
    int64_t i = 0;

    /* SIMD loop: process 4 doubles at a time */
    for (; i + 3 < n; i += 4) {
        __m256d x_vec = _mm256_loadu_pd(&x[i]);
        __m256d y_vec = _mm256_loadu_pd(&y[i]);
        sum_vec = _mm256_fmadd_pd(x_vec, y_vec, sum_vec);
    }

    /* Horizontal reduction */
    __m128d lo = _mm256_castpd256_pd128(sum_vec);
    __m128d hi = _mm256_extractf128_pd(sum_vec, 1);
    __m128d sum_pair = _mm_add_pd(lo, hi);
    double sum = _mm_cvtsd_f64(sum_pair) + _mm_cvtsd_f64(_mm_unpackhi_pd(sum_pair, sum_pair));

    /* Scalar tail */
    for (; i < n; i++) {
        sum += x[i] * y[i];
    }

    return sum;
}
#endif

#if FC_HAS_SSE42
static inline double fc_vec_dot_f64_sse42(const double* x, const double* y, int64_t n) {
    __m128d sum_vec = _mm_setzero_pd();
    int64_t i = 0;

    /* SIMD loop: process 2 doubles at a time */
    for (; i + 1 < n; i += 2) {
        __m128d x_vec = _mm_loadu_pd(&x[i]);
        __m128d y_vec = _mm_loadu_pd(&y[i]);
        sum_vec = _mm_add_pd(sum_vec, _mm_mul_pd(x_vec, y_vec));
    }

    /* Horizontal reduction */
    double sum = _mm_cvtsd_f64(sum_vec) + _mm_cvtsd_f64(_mm_unpackhi_pd(sum_vec, sum_vec));

    /* Scalar tail */
    for (; i < n; i++) {
        sum += x[i] * y[i];
    }

    return sum;
}
#endif

static inline double fc_vec_dot_f64_scalar(const double* x, const double* y, int64_t n) {
    double sum = 0.0;
    for (int64_t i = 0; i < n; i++) {
        sum += x[i] * y[i];
    }
    return sum;
}

/**
 * @brief Compute dot product with SIMD optimization
 *
 * Automatically selects optimal SIMD implementation based on CPU capabilities.
 */
int fc_vec_dot_f64(const double* x, const double* y, int64_t n, double* result) {
    if (result == NULL) {
        return FC_ERR_INVALID_ARG;
    }

    int status = validate_vector_inputs(x, y, n);
    if (status != FC_OK) {
        return status;
    }

    fc_simd_level_t level = fc_get_simd_level();

#if FC_HAS_AVX2
    if (level >= FC_SIMD_AVX2) {
        *result = fc_vec_dot_f64_avx2(x, y, n);
        return FC_OK;
    }
#endif

#if FC_HAS_SSE42
    if (level >= FC_SIMD_SSE42) {
        *result = fc_vec_dot_f64_sse42(x, y, n);
        return FC_OK;
    }
#endif

    *result = fc_vec_dot_f64_scalar(x, y, n);
    return FC_OK;
}

/*
 * L2 norm (Euclidean norm)
*/

#if FC_HAS_AVX2
static inline double fc_vec_norm_l2_f64_avx2(const double* x, int64_t n) {
    __m256d sum_vec = _mm256_setzero_pd();
    int64_t i = 0;

    /* SIMD loop: process 4 doubles at a time */
    for (; i + 3 < n; i += 4) {
        __m256d x_vec = _mm256_loadu_pd(&x[i]);
        sum_vec = _mm256_fmadd_pd(x_vec, x_vec, sum_vec);
    }

    /* Horizontal reduction */
    __m128d lo = _mm256_castpd256_pd128(sum_vec);
    __m128d hi = _mm256_extractf128_pd(sum_vec, 1);
    __m128d sum_pair = _mm_add_pd(lo, hi);
    double sum = _mm_cvtsd_f64(sum_pair) + _mm_cvtsd_f64(_mm_unpackhi_pd(sum_pair, sum_pair));

    /* Scalar tail */
    for (; i < n; i++) {
        sum += x[i] * x[i];
    }

    return sqrt(sum);
}
#endif

#if FC_HAS_SSE42
static inline double fc_vec_norm_l2_f64_sse42(const double* x, int64_t n) {
    __m128d sum_vec = _mm_setzero_pd();
    int64_t i = 0;

    /* SIMD loop: process 2 doubles at a time */
    for (; i + 1 < n; i += 2) {
        __m128d x_vec = _mm_loadu_pd(&x[i]);
        sum_vec = _mm_add_pd(sum_vec, _mm_mul_pd(x_vec, x_vec));
    }

    /* Horizontal reduction */
    double sum = _mm_cvtsd_f64(sum_vec) + _mm_cvtsd_f64(_mm_unpackhi_pd(sum_vec, sum_vec));

    /* Scalar tail */
    for (; i < n; i++) {
        sum += x[i] * x[i];
    }

    return sqrt(sum);
}
#endif

static inline double fc_vec_norm_l2_f64_scalar(const double* x, int64_t n) {
    double sum = 0.0;
    for (int64_t i = 0; i < n; i++) {
        sum += x[i] * x[i];
    }
    return sqrt(sum);
}

/**
 * @brief Compute L2 norm with SIMD optimization
 *
 * Automatically selects optimal SIMD implementation based on CPU capabilities.
 * Note: Simplified implementation without overflow protection for typical financial data ranges.
 */
int fc_vec_norm_l2_f64(const double* x, int64_t n, double* result) {
    if (result == NULL) {
        return FC_ERR_INVALID_ARG;
    }

    int status = validate_single_vector(x, n);
    if (status != FC_OK) {
        return status;
    }

    if (n == 1) {
        *result = fabs(x[0]);
        return FC_OK;
    }

    fc_simd_level_t level = fc_get_simd_level();

#if FC_HAS_AVX2
    if (level >= FC_SIMD_AVX2) {
        *result = fc_vec_norm_l2_f64_avx2(x, n);
        return FC_OK;
    }
#endif

#if FC_HAS_SSE42
    if (level >= FC_SIMD_SSE42) {
        *result = fc_vec_norm_l2_f64_sse42(x, n);
        return FC_OK;
    }
#endif

    *result = fc_vec_norm_l2_f64_scalar(x, n);
    return FC_OK;
}

/*
 * L1 norm (Manhattan norm)
*/

/**
 * @brief Compute L1 norm using Kahan summation
 */
int fc_vec_norm_l1_f64(const double* x, int64_t n, double* result) {
    if (result == NULL) {
        return FC_ERR_INVALID_ARG;
    }

    int status = validate_single_vector(x, n);
    if (status != FC_OK) {
        return status;
    }

    /* Kahan summation for numerical stability */
    double sum = 0.0;
    double c = 0.0;

    for (int64_t i = 0; i < n; i++) {
        double abs_val = fabs(x[i]);
        double y_val = abs_val - c;
        double t = sum + y_val;
        c = (t - sum) - y_val;
        sum = t;
    }

    *result = sum;
    return FC_OK;
}

/*
 * Euclidean distance
*/

/**
 * @brief Compute Euclidean distance with overflow protection
 *
 * Computes ||x - y||_2 using the same scaling technique as L2 norm.
 */
int fc_vec_distance_l2_f64(const double* x, const double* y, int64_t n, double* result) {
    if (result == NULL) {
        return FC_ERR_INVALID_ARG;
    }

    int status = validate_vector_inputs(x, y, n);
    if (status != FC_OK) {
        return status;
    }

    if (n == 1) {
        *result = fabs(x[0] - y[0]);
        return FC_OK;
    }

    /* Scale-based algorithm */
    double scale = 0.0;
    double ssq = 1.0;

    for (int64_t i = 0; i < n; i++) {
        double diff = x[i] - y[i];
        double abs_diff = fabs(diff);

        if (abs_diff > 0.0) {
            if (scale < abs_diff) {
                double ratio = scale / abs_diff;
                ssq = 1.0 + ssq * ratio * ratio;
                scale = abs_diff;
            } else {
                double ratio = abs_diff / scale;
                ssq += ratio * ratio;
            }
        }
    }

    *result = scale * sqrt(ssq);
    return FC_OK;
}

/*
 * Manhattan distance
*/

/**
 * @brief Compute Manhattan distance using Kahan summation
 */
int fc_vec_distance_l1_f64(const double* x, const double* y, int64_t n, double* result) {
    if (result == NULL) {
        return FC_ERR_INVALID_ARG;
    }

    int status = validate_vector_inputs(x, y, n);
    if (status != FC_OK) {
        return status;
    }

    /* Kahan summation */
    double sum = 0.0;
    double c = 0.0;

    for (int64_t i = 0; i < n; i++) {
        double abs_diff = fabs(x[i] - y[i]);
        double y_val = abs_diff - c;
        double t = sum + y_val;
        c = (t - sum) - y_val;
        sum = t;
    }

    *result = sum;
    return FC_OK;
}
