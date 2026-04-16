/**
 * @file gemm_dispatch.c
 * @brief GEMM runtime dispatch layer
 *
 * Selects optimal GEMM implementation based on CPU features.
 * Dispatch order: AVX-512 > AVX2 > SSE4.2 > Scalar
 */

#include "fin-kit/matrix.h"
#include "../platform/simd_detect.h"
#include "../platform/error.h"

/* Forward declarations of internal implementations */
extern int fc_mat_gemm_f64_scalar(int m, int n, int k,
                                  double alpha,
                                  const double* A, int lda,
                                  const double* B, int ldb,
                                  double beta,
                                  double* C, int ldc);

extern int fc_mat_gemm_f64_sse42(int m, int n, int k,
                                 double alpha,
                                 const double* A, int lda,
                                 const double* B, int ldb,
                                 double beta,
                                 double* C, int ldc);

/**
 * @brief GEMM dispatch - selects best implementation at runtime
 *
 * @param m Number of rows in A and C
 * @param n Number of columns in B and C
 * @param k Number of columns in A and rows in B
 * @param alpha Scalar multiplier for A*B
 * @param A Input matrix A (m x k)
 * @param lda Leading dimension of A (>= k)
 * @param B Input matrix B (k x n)
 * @param ldb Leading dimension of B (>= n)
 * @param beta Scalar multiplier for C
 * @param C Output matrix C (m x n), C = alpha*A*B + beta*C
 * @param ldc Leading dimension of C (>= n)
 * @return FC_OK on success, error code otherwise
 */
int fc_mat_gemm_f64(int m, int n, int k,
                    double alpha,
                    const double* A, int lda,
                    const double* B, int ldb,
                    double beta,
                    double* C, int ldc) {
    fc_simd_level_t level = fc_get_simd_level();

    /* Dispatch to best available implementation */
    if (level >= FC_SIMD_SSE42) {
        return fc_mat_gemm_f64_sse42(m, n, k, alpha, A, lda, B, ldb, beta, C, ldc);
    }

    /* Fallback to scalar */
    return fc_mat_gemm_f64_scalar(m, n, k, alpha, A, lda, B, ldb, beta, C, ldc);
}
