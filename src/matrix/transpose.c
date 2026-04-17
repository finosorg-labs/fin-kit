/**
 * @file transpose.c
 * @brief Matrix transpose implementation
 *
 * Implements cache-friendly matrix transpose with SIMD optimization.
 * Uses blocking to improve cache locality for large matrices.
 */

#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <fin-kit/platform/platform.h>
#include <fin-kit/platform/error.h>
#include <fin-kit/platform/simd_detect.h>
#include <fin-kit/matrix/matrix_internal.h>

#define TRANSPOSE_BLOCK_SIZE 32

#if FC_HAS_AVX2
#include <immintrin.h>

static inline void transpose_4x4_avx2(const double* src, int ld_src,
                                       double* dst, int ld_dst) {
    __m256d row0 = _mm256_loadu_pd(&src[0 * ld_src]);
    __m256d row1 = _mm256_loadu_pd(&src[1 * ld_src]);
    __m256d row2 = _mm256_loadu_pd(&src[2 * ld_src]);
    __m256d row3 = _mm256_loadu_pd(&src[3 * ld_src]);

    __m256d tmp0 = _mm256_unpacklo_pd(row0, row1);
    __m256d tmp1 = _mm256_unpackhi_pd(row0, row1);
    __m256d tmp2 = _mm256_unpacklo_pd(row2, row3);
    __m256d tmp3 = _mm256_unpackhi_pd(row2, row3);

    __m256d col0 = _mm256_permute2f128_pd(tmp0, tmp2, 0x20);
    __m256d col1 = _mm256_permute2f128_pd(tmp1, tmp3, 0x20);
    __m256d col2 = _mm256_permute2f128_pd(tmp0, tmp2, 0x31);
    __m256d col3 = _mm256_permute2f128_pd(tmp1, tmp3, 0x31);

    _mm256_storeu_pd(&dst[0 * ld_dst], col0);
    _mm256_storeu_pd(&dst[1 * ld_dst], col1);
    _mm256_storeu_pd(&dst[2 * ld_dst], col2);
    _mm256_storeu_pd(&dst[3 * ld_dst], col3);
}

static void transpose_block_avx2(int rows, int cols,
                                  const double* src, int ld_src,
                                  double* dst, int ld_dst) {
    int i = 0;
    for (; i + 3 < rows && i + 3 < cols; i += 4) {
        int j = 0;
        for (; j + 3 < cols; j += 4) {
            transpose_4x4_avx2(&src[i * ld_src + j], ld_src,
                               &dst[j * ld_dst + i], ld_dst);
        }

        for (; j < cols; j++) {
            for (int ii = 0; ii < 4 && i + ii < rows; ii++) {
                dst[j * ld_dst + i + ii] = src[(i + ii) * ld_src + j];
            }
        }
    }

    for (; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            dst[j * ld_dst + i] = src[i * ld_src + j];
        }
    }
}

#endif

static void transpose_scalar(int rows, int cols,
                              const double* src, int ld_src,
                              double* dst, int ld_dst) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            dst[j * ld_dst + i] = src[i * ld_src + j];
        }
    }
}

#ifdef FC_ENABLE_INTERNAL_TESTS
void
#else
static void
#endif
transpose_blocked(int rows, int cols,
                  const double* src, int ld_src,
                  double* dst, int ld_dst) {
    for (int i = 0; i < rows; i += TRANSPOSE_BLOCK_SIZE) {
        for (int j = 0; j < cols; j += TRANSPOSE_BLOCK_SIZE) {
            int block_rows = (i + TRANSPOSE_BLOCK_SIZE <= rows) ? TRANSPOSE_BLOCK_SIZE : (rows - i);
            int block_cols = (j + TRANSPOSE_BLOCK_SIZE <= cols) ? TRANSPOSE_BLOCK_SIZE : (cols - j);

            for (int ii = 0; ii < block_rows; ii++) {
                for (int jj = 0; jj < block_cols; jj++) {
                    dst[(j + jj) * ld_dst + i + ii] = src[(i + ii) * ld_src + j + jj];
                }
            }
        }
    }
}

int fc_mat_transpose_f64(int64_t rows, int64_t cols,
                         const double* src, int64_t ld_src,
                         double* dst, int64_t ld_dst) {
    if (rows <= 0 || cols <= 0) {
        return FC_ERR_INVALID_ARG;
    }
    if (src == NULL || dst == NULL) {
        return FC_ERR_INVALID_ARG;
    }
    if (ld_src < cols || ld_dst < rows) {
        return FC_ERR_INVALID_ARG;
    }
    if (rows > INT_MAX || cols > INT_MAX || ld_src > INT_MAX || ld_dst > INT_MAX) {
        return FC_ERR_INVALID_ARG;
    }

    int rows_int = (int)rows;
    int cols_int = (int)cols;
    int ld_src_int = (int)ld_src;
    int ld_dst_int = (int)ld_dst;

#if FC_HAS_AVX2 || FC_HAS_SSE42
    fc_simd_level_t level = fc_get_simd_level();
#endif

#if FC_HAS_AVX2
    if (level >= FC_SIMD_AVX2 && rows_int >= 4 && cols_int >= 4) {
        for (int i = 0; i < rows_int; i += TRANSPOSE_BLOCK_SIZE) {
            for (int j = 0; j < cols_int; j += TRANSPOSE_BLOCK_SIZE) {
                int block_rows = (i + TRANSPOSE_BLOCK_SIZE <= rows_int) ? TRANSPOSE_BLOCK_SIZE : (rows_int - i);
                int block_cols = (j + TRANSPOSE_BLOCK_SIZE <= cols_int) ? TRANSPOSE_BLOCK_SIZE : (cols_int - j);

                transpose_block_avx2(block_rows, block_cols,
                                     &src[i * ld_src_int + j], ld_src_int,
                                     &dst[j * ld_dst_int + i], ld_dst_int);
            }
        }
        return FC_OK;
    }
#endif

    if (rows_int >= TRANSPOSE_BLOCK_SIZE || cols_int >= TRANSPOSE_BLOCK_SIZE) {
        transpose_blocked(rows_int, cols_int, src, ld_src_int, dst, ld_dst_int);
    } else {
        transpose_scalar(rows_int, cols_int, src, ld_src_int, dst, ld_dst_int);
    }

    return FC_OK;
}
