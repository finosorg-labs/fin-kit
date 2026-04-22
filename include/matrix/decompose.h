/**
 * @file decompose.h
 * @brief Matrix decomposition operations
 *
 * Provides LU, QR, and Cholesky decompositions.
 */

#ifndef FC_DECOMPOSE_H
#define FC_DECOMPOSE_H

#include <version.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif /* FC_DECOMPOSE_H */
