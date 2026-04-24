/**
 * @file gemm.h
 * @brief General Matrix Multiply (GEMM) operations
 *
 * Provides high-performance matrix multiplication with SIMD acceleration.
 * Automatically selects optimal implementation based on CPU capabilities.
 */

#ifndef FC_GEMM_H
#define FC_GEMM_H

#include "version.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief General Matrix Multiply (GEMM) for double-precision matrices
 *
 * Computes: C = alpha * A * B + beta * C
 * where A is m×k, B is k×n, C is m×n
 *
 * Matrices are stored in row-major order.
 * Automatically selects optimal SIMD implementation based on CPU capabilities.
 *
 * Time complexity: O(m * n * k)
 * Space complexity: O(1) (in-place update of C)
 * Thread safety: Thread-safe (no shared state)
 *
 * @param[in]    m      Number of rows in A and C
 * @param[in]    n      Number of columns in B and C
 * @param[in]    k      Number of columns in A and rows in B
 * @param[in]    alpha  Scalar multiplier for A*B
 * @param[in]    A      Matrix A (m×k, row-major)
 * @param[in]    lda    Leading dimension of A (stride, typically k)
 * @param[in]    B      Matrix B (k×n, row-major)
 * @param[in]    ldb    Leading dimension of B (stride, typically n)
 * @param[in]    beta   Scalar multiplier for C
 * @param[inout] C      Matrix C (m×n, row-major), updated in-place
 * @param[in]    ldc    Leading dimension of C (stride, typically n)
 *
 * @return FC_OK on success, FC_ERR_INVALID_ARG if inputs invalid
 */
int fc_mat_gemm_f64(int64_t m, int64_t n, int64_t k,
                    double alpha,
                    const double* A, int64_t lda,
                    const double* B, int64_t ldb,
                    double beta,
                    double* C, int64_t ldc);

#ifdef __cplusplus
}
#endif

#endif /* FC_GEMM_H */
