/**
 * @file gemm_dispatch.c
 * @brief GEMM runtime dispatch layer
 *
 * Selects optimal GEMM implementation based on CPU features.
 * Dispatch order: AVX-512 > AVX2 > SSE4.2 > Scalar
 *
 * The public API accepts int64_t dimensions for future-proofing.
 * Internal SIMD kernels use int for loop indices (sufficient for
 * practical matrix sizes and avoids SIMD register width issues).
 */

#include <matrix/matrix.h>
#include <simd_detect.h>
#include <error.h>

#include <limits.h>

/* Forward declarations of internal implementations (int dimensions) */
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

extern int fc_mat_gemm_f64_avx2(int m, int n, int k,
                                double alpha,
                                const double* A, int lda,
                                const double* B, int ldb,
                                double beta,
                                double* C, int ldc);

extern int fc_mat_gemm_f64_avx512(int m, int n, int k,
                                  double alpha,
                                  const double* A, int lda,
                                  const double* B, int ldb,
                                  double beta,
                                  double* C, int ldc);

int fc_mat_gemm_f64(int64_t m, int64_t n, int64_t k,
                    double alpha,
                    const double* A, int64_t lda,
                    const double* B, int64_t ldb,
                    double beta,
                    double* C, int64_t ldc) {
    if (m <= 0 || n <= 0 || k <= 0) {
        return FC_ERR_INVALID_ARG;
    }
    if (m > INT_MAX || n > INT_MAX || k > INT_MAX ||
        lda > INT_MAX || ldb > INT_MAX || ldc > INT_MAX) {
        return FC_ERR_INVALID_ARG;
    }

    int mi = (int)m, ni = (int)n, ki = (int)k;
    int ldai = (int)lda, ldbi = (int)ldb, ldci = (int)ldc;

    fc_simd_level_t level = fc_get_simd_level();

    if (level >= FC_SIMD_AVX512) {
        return fc_mat_gemm_f64_avx512(mi, ni, ki, alpha, A, ldai, B, ldbi, beta, C, ldci);
    }

    if (level >= FC_SIMD_AVX2) {
        return fc_mat_gemm_f64_avx2(mi, ni, ki, alpha, A, ldai, B, ldbi, beta, C, ldci);
    }

    if (level >= FC_SIMD_SSE42) {
        return fc_mat_gemm_f64_sse42(mi, ni, ki, alpha, A, ldai, B, ldbi, beta, C, ldci);
    }

    return fc_mat_gemm_f64_scalar(mi, ni, ki, alpha, A, ldai, B, ldbi, beta, C, ldci);
}
