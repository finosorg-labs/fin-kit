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

/*
 * Matrix decompositions
*/

/**
 * @brief LU decomposition with partial pivoting
 *
 * Computes: P * A = L * U
 * where A is n×n, L is lower triangular with unit diagonal,
 * U is upper triangular, P is permutation matrix
 *
 * The L and U factors are stored in-place in matrix A:
 * - Upper triangle (including diagonal) contains U
 * - Strict lower triangle contains L (diagonal of L is implicitly 1)
 *
 * Time complexity: O(n^3)
 * Space complexity: O(n) for pivot array
 * Thread safety: Thread-safe (no shared state)
 *
 * @param[in]    n     Matrix dimension (must be > 0)
 * @param[inout] A     Input matrix (n×n, row-major), overwritten with L and U
 * @param[in]    lda   Leading dimension of A (stride, typically n)
 * @param[out]   ipiv  Pivot indices array (length n), ipiv[i] = row swapped with row i
 *
 * @return FC_OK on success, FC_ERR_INVALID_ARG if inputs invalid,
 *         FC_ERR_SINGULAR_MATRIX if matrix is singular
 */
int fc_mat_lu_decompose_f64(int64_t n, double* A, int64_t lda, int64_t* ipiv);

/**
 * @brief QR decomposition using Householder reflections
 *
 * Computes: A = Q * R
 * where A is m×n (m >= n), Q is m×m orthogonal, R is m×n upper triangular
 *
 * The R factor is stored in the upper triangle of A.
 * The Householder vectors are stored in the lower triangle of A.
 * The tau array contains scaling factors for the Householder reflectors.
 *
 * Time complexity: O(m * n^2)
 * Space complexity: O(n) for tau array
 * Thread safety: Thread-safe (no shared state)
 *
 * @param[in]    m     Number of rows in A (must be >= n)
 * @param[in]    n     Number of columns in A (must be > 0)
 * @param[inout] A     Input matrix (m×n, row-major), overwritten with Q and R factors
 * @param[in]    lda   Leading dimension of A (stride, typically n)
 * @param[out]   tau   Householder scaling factors (length n)
 *
 * @return FC_OK on success, FC_ERR_INVALID_ARG if inputs invalid,
 *         FC_ERR_DIMENSION_MISMATCH if m < n
 */
int fc_mat_qr_decompose_f64(int64_t m, int64_t n, double* A, int64_t lda, double* tau);

/**
 * @brief Cholesky decomposition for symmetric positive definite matrices
 *
 * Computes: A = L * L^T
 * where A is n×n symmetric positive definite, L is lower triangular
 *
 * Only the lower triangle of A is accessed.
 * The L factor is stored in the lower triangle of A (upper triangle unchanged).
 *
 * Time complexity: O(n^3)
 * Space complexity: O(1) (in-place)
 * Thread safety: Thread-safe (no shared state)
 *
 * @param[in]    n     Matrix dimension (must be > 0)
 * @param[inout] A     Input matrix (n×n, row-major), lower triangle overwritten with L
 * @param[in]    lda   Leading dimension of A (stride, typically n)
 *
 * @return FC_OK on success, FC_ERR_INVALID_ARG if inputs invalid,
 *         FC_ERR_NOT_POSITIVE_DEF if matrix is not positive definite
 */
int fc_mat_cholesky_decompose_f64(int64_t n, double* A, int64_t lda);

/*
 * Linear system solvers
*/

/**
 * @brief Solve linear system A * X = B using LU decomposition
 *
 * Computes: X = A^(-1) * B
 * where A is n×n, B is n×nrhs, X is n×nrhs
 *
 * The matrix A is overwritten with its LU decomposition.
 * The matrix B is overwritten with the solution X.
 *
 * Time complexity: O(n^3 + n^2 * nrhs)
 * Space complexity: O(n) for pivot array
 * Thread safety: Thread-safe (no shared state)
 *
 * @param[in]    n     Matrix dimension (must be > 0)
 * @param[in]    nrhs  Number of right-hand sides (must be > 0)
 * @param[inout] A     Coefficient matrix (n×n, row-major), overwritten with LU factors
 * @param[in]    lda   Leading dimension of A (stride, typically n)
 * @param[inout] B     Right-hand side matrix (n×nrhs, row-major), overwritten with solution
 * @param[in]    ldb   Leading dimension of B (stride, typically nrhs)
 *
 * @return FC_OK on success, FC_ERR_INVALID_ARG if inputs invalid,
 *         FC_ERR_SINGULAR_MATRIX if matrix is singular
 */
int fc_mat_solve_linear_f64(int64_t n, int64_t nrhs,
                            double* A, int64_t lda,
                            double* B, int64_t ldb);

/**
 * @brief Compute matrix inverse using LU decomposition
 *
 * Computes: A_inv = A^(-1)
 * where A is n×n non-singular
 *
 * The input matrix A is overwritten with its inverse.
 *
 * Time complexity: O(n^3)
 * Space complexity: O(n^2) for workspace
 * Thread safety: Thread-safe (no shared state)
 *
 * @param[in]    n     Matrix dimension (must be > 0)
 * @param[inout] A     Input matrix (n×n, row-major), overwritten with inverse
 * @param[in]    lda   Leading dimension of A (stride, typically n)
 *
 * @return FC_OK on success, FC_ERR_INVALID_ARG if inputs invalid,
 *         FC_ERR_SINGULAR_MATRIX if matrix is singular
 */
int fc_mat_inverse_f64(int64_t n, double* A, int64_t lda);

#ifdef __cplusplus
}
#endif

#endif /* FC_MATRIX_H */
