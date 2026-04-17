/**
 * @file matrix.h
 * @brief Matrix and linear algebra operations
 *
 * Provides high-performance matrix operations including:
 * - GEMM (General Matrix Multiply)
 * - GEMV (General Matrix-Vector Multiply)
 * - Vector operations (dot product, norms, distances)
 * - Matrix decompositions (LU, QR, Cholesky, SVD)
 * - Linear system solvers
 *
 * All operations support batch processing for optimal performance.
 * SIMD acceleration is automatically selected based on CPU capabilities.
 */

#ifndef FC_MATRIX_H
#define FC_MATRIX_H

#include <fin-kit/version.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * Vector operations
*/

/**
 * @brief Compute dot product of two double-precision vectors
 *
 * Computes: result = sum(x[i] * y[i]) for i in [0, n)
 *
 * Time complexity: O(n)
 * Space complexity: O(1)
 * Thread safety: Thread-safe (no shared state)
 *
 * @param[in]  x      First vector (length n)
 * @param[in]  y      Second vector (length n)
 * @param[in]  n      Vector length (must be > 0)
 * @param[out] result Dot product result
 *
 * @return FC_OK on success, FC_ERR_INVALID_ARG if inputs invalid
 */
int fc_vec_dot_f64(const double* x, const double* y, int64_t n, double* result);

/**
 * @brief Compute L2 norm (Euclidean norm) of a double-precision vector
 *
 * Computes: result = sqrt(sum(x[i]^2))
 *
 * Time complexity: O(n)
 * Space complexity: O(1)
 * Thread safety: Thread-safe
 *
 * @param[in]  x      Input vector (length n)
 * @param[in]  n      Vector length (must be > 0)
 * @param[out] result L2 norm result
 *
 * @return FC_OK on success, FC_ERR_INVALID_ARG if inputs invalid
 */
int fc_vec_norm_l2_f64(const double* x, int64_t n, double* result);

/**
 * @brief Compute L1 norm (Manhattan norm) of a double-precision vector
 *
 * Computes: result = sum(|x[i]|)
 *
 * Time complexity: O(n)
 * Space complexity: O(1)
 * Thread safety: Thread-safe
 *
 * @param[in]  x      Input vector (length n)
 * @param[in]  n      Vector length (must be > 0)
 * @param[out] result L1 norm result
 *
 * @return FC_OK on success, FC_ERR_INVALID_ARG if inputs invalid
 */
int fc_vec_norm_l1_f64(const double* x, int64_t n, double* result);

/**
 * @brief Compute Euclidean distance between two double-precision vectors
 *
 * Computes: result = sqrt(sum((x[i] - y[i])^2))
 *
 * Time complexity: O(n)
 * Space complexity: O(1)
 * Thread safety: Thread-safe
 *
 * @param[in]  x      First vector (length n)
 * @param[in]  y      Second vector (length n)
 * @param[in]  n      Vector length (must be > 0)
 * @param[out] result Euclidean distance result
 *
 * @return FC_OK on success, FC_ERR_INVALID_ARG if inputs invalid
 */
int fc_vec_distance_l2_f64(const double* x, const double* y, int64_t n, double* result);

/**
 * @brief Compute Manhattan distance between two double-precision vectors
 *
 * Computes: result = sum(|x[i] - y[i]|)
 *
 * Time complexity: O(n)
 * Space complexity: O(1)
 * Thread safety: Thread-safe
 *
 * @param[in]  x      First vector (length n)
 * @param[in]  y      Second vector (length n)
 * @param[in]  n      Vector length (must be > 0)
 * @param[out] result Manhattan distance result
 *
 * @return FC_OK on success, FC_ERR_INVALID_ARG if inputs invalid
 */
int fc_vec_distance_l1_f64(const double* x, const double* y, int64_t n, double* result);

/*
 * Matrix multiplication
*/

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

/*
 * Matrix-vector multiplication
*/

/**
 * @brief General Matrix-Vector Multiply (GEMV) for double-precision
 *
 * Computes: y = alpha * A * x + beta * y
 * where A is m×n, x is n×1, y is m×1
 *
 * Matrix A is stored in row-major order.
 * Automatically selects optimal SIMD implementation based on CPU capabilities.
 *
 * Time complexity: O(m * n)
 * Space complexity: O(1) (in-place update of y)
 * Thread safety: Thread-safe (no shared state)
 *
 * @param[in]    m      Number of rows in A
 * @param[in]    n      Number of columns in A
 * @param[in]    alpha  Scalar multiplier for A*x
 * @param[in]    A      Matrix A (m×n, row-major)
 * @param[in]    lda    Leading dimension of A (stride, typically n)
 * @param[in]    x      Vector x (length n)
 * @param[in]    beta   Scalar multiplier for y
 * @param[inout] y      Vector y (length m), updated in-place
 *
 * @return FC_OK on success, FC_ERR_INVALID_ARG if inputs invalid
 */
int fc_mat_gemv_f64(int64_t m, int64_t n,
                    double alpha,
                    const double* A, int64_t lda,
                    const double* x,
                    double beta,
                    double* y);

/*
 * Matrix transpose
*/

/**
 * @brief Transpose a double-precision matrix
 *
 * Computes: dst = src^T
 * where src is rows×cols, dst is cols×rows
 *
 * Uses cache-friendly blocking for large matrices.
 * Automatically selects optimal SIMD implementation based on CPU capabilities.
 *
 * Time complexity: O(rows * cols)
 * Space complexity: O(1)
 * Thread safety: Thread-safe (no shared state)
 *
 * @param[in]  rows   Number of rows in source matrix
 * @param[in]  cols   Number of columns in source matrix
 * @param[in]  src    Source matrix (rows×cols, row-major)
 * @param[in]  ld_src Leading dimension of source (stride between rows)
 * @param[out] dst    Destination matrix (cols×rows, row-major)
 * @param[in]  ld_dst Leading dimension of destination (stride between rows)
 *
 * @return FC_OK on success, FC_ERR_INVALID_ARG if inputs invalid
 */
int fc_mat_transpose_f64(int64_t rows, int64_t cols,
                         const double* src, int64_t ld_src,
                         double* dst, int64_t ld_dst);

#ifdef __cplusplus
}
#endif

#endif /* FC_MATRIX_H */
