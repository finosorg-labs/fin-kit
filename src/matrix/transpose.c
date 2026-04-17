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

static inline void transpose_8x8_avx2(const double* src, int ld_src,
                                       double* dst, int ld_dst) {
    // Load 8 rows, each row has 8 doubles (need 2 AVX2 loads per row)
    __m256d r0_lo = _mm256_loadu_pd(&src[0 * ld_src + 0]);
    __m256d r0_hi = _mm256_loadu_pd(&src[0 * ld_src + 4]);
    __m256d r1_lo = _mm256_loadu_pd(&src[1 * ld_src + 0]);
    __m256d r1_hi = _mm256_loadu_pd(&src[1 * ld_src + 4]);
    __m256d r2_lo = _mm256_loadu_pd(&src[2 * ld_src + 0]);
    __m256d r2_hi = _mm256_loadu_pd(&src[2 * ld_src + 4]);
    __m256d r3_lo = _mm256_loadu_pd(&src[3 * ld_src + 0]);
    __m256d r3_hi = _mm256_loadu_pd(&src[3 * ld_src + 4]);
    __m256d r4_lo = _mm256_loadu_pd(&src[4 * ld_src + 0]);
    __m256d r4_hi = _mm256_loadu_pd(&src[4 * ld_src + 4]);
    __m256d r5_lo = _mm256_loadu_pd(&src[5 * ld_src + 0]);
    __m256d r5_hi = _mm256_loadu_pd(&src[5 * ld_src + 4]);
    __m256d r6_lo = _mm256_loadu_pd(&src[6 * ld_src + 0]);
    __m256d r6_hi = _mm256_loadu_pd(&src[6 * ld_src + 4]);
    __m256d r7_lo = _mm256_loadu_pd(&src[7 * ld_src + 0]);
    __m256d r7_hi = _mm256_loadu_pd(&src[7 * ld_src + 4]);

    // Transpose the left 4×8 block (rows 0-7, cols 0-3)
    __m256d t0 = _mm256_unpacklo_pd(r0_lo, r1_lo);
    __m256d t1 = _mm256_unpackhi_pd(r0_lo, r1_lo);
    __m256d t2 = _mm256_unpacklo_pd(r2_lo, r3_lo);
    __m256d t3 = _mm256_unpackhi_pd(r2_lo, r3_lo);
    __m256d t4 = _mm256_unpacklo_pd(r4_lo, r5_lo);
    __m256d t5 = _mm256_unpackhi_pd(r4_lo, r5_lo);
    __m256d t6 = _mm256_unpacklo_pd(r6_lo, r7_lo);
    __m256d t7 = _mm256_unpackhi_pd(r6_lo, r7_lo);

    __m256d c0 = _mm256_permute2f128_pd(t0, t2, 0x20);
    __m256d c1 = _mm256_permute2f128_pd(t1, t3, 0x20);
    __m256d c2 = _mm256_permute2f128_pd(t0, t2, 0x31);
    __m256d c3 = _mm256_permute2f128_pd(t1, t3, 0x31);
    __m256d c4 = _mm256_permute2f128_pd(t4, t6, 0x20);
    __m256d c5 = _mm256_permute2f128_pd(t5, t7, 0x20);
    __m256d c6 = _mm256_permute2f128_pd(t4, t6, 0x31);
    __m256d c7 = _mm256_permute2f128_pd(t5, t7, 0x31);

    // Transpose the right 4×8 block (rows 0-7, cols 4-7)
    __m256d t8 = _mm256_unpacklo_pd(r0_hi, r1_hi);
    __m256d t9 = _mm256_unpackhi_pd(r0_hi, r1_hi);
    __m256d t10 = _mm256_unpacklo_pd(r2_hi, r3_hi);
    __m256d t11 = _mm256_unpackhi_pd(r2_hi, r3_hi);
    __m256d t12 = _mm256_unpacklo_pd(r4_hi, r5_hi);
    __m256d t13 = _mm256_unpackhi_pd(r4_hi, r5_hi);
    __m256d t14 = _mm256_unpacklo_pd(r6_hi, r7_hi);
    __m256d t15 = _mm256_unpackhi_pd(r6_hi, r7_hi);

    __m256d c8 = _mm256_permute2f128_pd(t8, t10, 0x20);
    __m256d c9 = _mm256_permute2f128_pd(t9, t11, 0x20);
    __m256d c10 = _mm256_permute2f128_pd(t8, t10, 0x31);
    __m256d c11 = _mm256_permute2f128_pd(t9, t11, 0x31);
    __m256d c12 = _mm256_permute2f128_pd(t12, t14, 0x20);
    __m256d c13 = _mm256_permute2f128_pd(t13, t15, 0x20);
    __m256d c14 = _mm256_permute2f128_pd(t12, t14, 0x31);
    __m256d c15 = _mm256_permute2f128_pd(t13, t15, 0x31);

    // Store transposed 8×8 block
    _mm256_storeu_pd(&dst[0 * ld_dst + 0], c0);
    _mm256_storeu_pd(&dst[0 * ld_dst + 4], c4);
    _mm256_storeu_pd(&dst[1 * ld_dst + 0], c1);
    _mm256_storeu_pd(&dst[1 * ld_dst + 4], c5);
    _mm256_storeu_pd(&dst[2 * ld_dst + 0], c2);
    _mm256_storeu_pd(&dst[2 * ld_dst + 4], c6);
    _mm256_storeu_pd(&dst[3 * ld_dst + 0], c3);
    _mm256_storeu_pd(&dst[3 * ld_dst + 4], c7);
    _mm256_storeu_pd(&dst[4 * ld_dst + 0], c8);
    _mm256_storeu_pd(&dst[4 * ld_dst + 4], c12);
    _mm256_storeu_pd(&dst[5 * ld_dst + 0], c9);
    _mm256_storeu_pd(&dst[5 * ld_dst + 4], c13);
    _mm256_storeu_pd(&dst[6 * ld_dst + 0], c10);
    _mm256_storeu_pd(&dst[6 * ld_dst + 4], c14);
    _mm256_storeu_pd(&dst[7 * ld_dst + 0], c11);
    _mm256_storeu_pd(&dst[7 * ld_dst + 4], c15);
}

static void transpose_block_avx2(int rows, int cols,
                                  const double* src, int ld_src,
                                  double* dst, int ld_dst) {
    int i = 0;

    /* Process 4×4 blocks */
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

    /* Scalar cleanup for remaining rows */
    for (; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            dst[j * ld_dst + i] = src[i * ld_src + j];
        }
    }
}

#endif

#if FC_HAS_SSE42
#include <emmintrin.h>
#include <smmintrin.h>

static inline void transpose_4x4_sse42(const double* src, int ld_src,
                                        double* dst, int ld_dst) {
    /* Load 4 rows, each with 4 doubles (need 2 SSE loads per row) */
    __m128d r0_lo = _mm_loadu_pd(&src[0 * ld_src + 0]);
    __m128d r0_hi = _mm_loadu_pd(&src[0 * ld_src + 2]);
    __m128d r1_lo = _mm_loadu_pd(&src[1 * ld_src + 0]);
    __m128d r1_hi = _mm_loadu_pd(&src[1 * ld_src + 2]);
    __m128d r2_lo = _mm_loadu_pd(&src[2 * ld_src + 0]);
    __m128d r2_hi = _mm_loadu_pd(&src[2 * ld_src + 2]);
    __m128d r3_lo = _mm_loadu_pd(&src[3 * ld_src + 0]);
    __m128d r3_hi = _mm_loadu_pd(&src[3 * ld_src + 2]);

    /* Transpose 2×2 blocks */
    __m128d t0 = _mm_unpacklo_pd(r0_lo, r1_lo);  /* [r0[0], r1[0]] */
    __m128d t1 = _mm_unpackhi_pd(r0_lo, r1_lo);  /* [r0[1], r1[1]] */
    __m128d t2 = _mm_unpacklo_pd(r2_lo, r3_lo);  /* [r2[0], r3[0]] */
    __m128d t3 = _mm_unpackhi_pd(r2_lo, r3_lo);  /* [r2[1], r3[1]] */
    __m128d t4 = _mm_unpacklo_pd(r0_hi, r1_hi);  /* [r0[2], r1[2]] */
    __m128d t5 = _mm_unpackhi_pd(r0_hi, r1_hi);  /* [r0[3], r1[3]] */
    __m128d t6 = _mm_unpacklo_pd(r2_hi, r3_hi);  /* [r2[2], r3[2]] */
    __m128d t7 = _mm_unpackhi_pd(r2_hi, r3_hi);  /* [r2[3], r3[3]] */

    /* Store transposed 4×4 block */
    _mm_storeu_pd(&dst[0 * ld_dst + 0], t0);
    _mm_storeu_pd(&dst[0 * ld_dst + 2], t2);
    _mm_storeu_pd(&dst[1 * ld_dst + 0], t1);
    _mm_storeu_pd(&dst[1 * ld_dst + 2], t3);
    _mm_storeu_pd(&dst[2 * ld_dst + 0], t4);
    _mm_storeu_pd(&dst[2 * ld_dst + 2], t6);
    _mm_storeu_pd(&dst[3 * ld_dst + 0], t5);
    _mm_storeu_pd(&dst[3 * ld_dst + 2], t7);
}

static void transpose_block_sse42(int rows, int cols,
                                   const double* src, int ld_src,
                                   double* dst, int ld_dst) {
    int i = 0;
    for (; i + 3 < rows && i + 3 < cols; i += 4) {
        int j = 0;
        for (; j + 3 < cols; j += 4) {
            transpose_4x4_sse42(&src[i * ld_src + j], ld_src,
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

#if FC_HAS_SSE42
    if (0 && level >= FC_SIMD_SSE42 && rows_int >= 4 && cols_int >= 4) {
        for (int i = 0; i < rows_int; i += TRANSPOSE_BLOCK_SIZE) {
            for (int j = 0; j < cols_int; j += TRANSPOSE_BLOCK_SIZE) {
                int block_rows = (i + TRANSPOSE_BLOCK_SIZE <= rows_int) ? TRANSPOSE_BLOCK_SIZE : (rows_int - i);
                int block_cols = (j + TRANSPOSE_BLOCK_SIZE <= cols_int) ? TRANSPOSE_BLOCK_SIZE : (cols_int - j);

                transpose_block_sse42(block_rows, block_cols,
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
