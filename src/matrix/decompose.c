/**
 * @file decompose.c
 * @brief Matrix decomposition implementations (LU, QR, Cholesky)
 *
 * Provides numerically stable implementations of matrix factorizations
 * with proper error handling and IEEE 754 compliance.
 */

#include <math.h>
#include <string.h>
#include <float.h>
#include <matrix/matrix.h>
#include <platform/platform.h>
#include <platform/error.h>

/*
 * Helper functions
*/

static FC_INLINE void swap_rows(double* A, int64_t lda, int64_t row1, int64_t row2, int64_t n) {
    if (row1 == row2) return;

    double* r1 = A + row1 * lda;
    double* r2 = A + row2 * lda;

    for (int64_t j = 0; j < n; j++) {
        double temp = r1[j];
        r1[j] = r2[j];
        r2[j] = temp;
    }
}

static FC_INLINE int64_t find_pivot(const double* A, int64_t lda, int64_t col, int64_t n) {
    int64_t pivot_row = col;
    double max_val = fabs(A[col * lda + col]);

    for (int64_t i = col + 1; i < n; i++) {
        double val = fabs(A[i * lda + col]);
        if (val > max_val) {
            max_val = val;
            pivot_row = i;
        }
    }

    return pivot_row;
}

/*
 * LU decomposition with partial pivoting
*/

int fc_mat_lu_decompose_f64(int64_t n, double* A, int64_t lda, int64_t* ipiv) {
    if (A == NULL || ipiv == NULL) {
        return FC_ERR_INVALID_ARG;
    }
    if (n <= 0) {
        return FC_ERR_INVALID_ARG;
    }
    if (lda < n) {
        return FC_ERR_INVALID_ARG;
    }

    /* Initialize pivot array */
    for (int64_t i = 0; i < n; i++) {
        ipiv[i] = i;
    }

    /* Gaussian elimination with partial pivoting */
    for (int64_t k = 0; k < n - 1; k++) {
        /* Find pivot */
        int64_t pivot_row = find_pivot(A, lda, k, n);

        /* Check for singularity */
        if (fabs(A[pivot_row * lda + k]) < DBL_EPSILON) {
            return FC_ERR_SINGULAR_MATRIX;
        }

        /* Swap rows if needed */
        if (pivot_row != k) {
            swap_rows(A, lda, k, pivot_row, n);
            int64_t temp = ipiv[k];
            ipiv[k] = ipiv[pivot_row];
            ipiv[pivot_row] = temp;
        }

        /* Compute multipliers and update submatrix */
        double pivot = A[k * lda + k];
        for (int64_t i = k + 1; i < n; i++) {
            double mult = A[i * lda + k] / pivot;
            A[i * lda + k] = mult;

            /* Update row i */
            for (int64_t j = k + 1; j < n; j++) {
                A[i * lda + j] -= mult * A[k * lda + j];
            }
        }
    }

    /* Check final diagonal element */
    if (fabs(A[(n-1) * lda + (n-1)]) < DBL_EPSILON) {
        return FC_ERR_SINGULAR_MATRIX;
    }

    return FC_OK;
}

/*
 * QR decomposition using Householder reflections
*/

int fc_mat_qr_decompose_f64(int64_t m, int64_t n, double* A, int64_t lda, double* tau) {
    if (A == NULL || tau == NULL) {
        return FC_ERR_INVALID_ARG;
    }
    if (m <= 0 || n <= 0) {
        return FC_ERR_INVALID_ARG;
    }
    if (m < n) {
        return FC_ERR_DIMENSION_MISMATCH;
    }
    if (lda < n) {
        return FC_ERR_INVALID_ARG;
    }

    /* Householder QR factorization */
    for (int64_t k = 0; k < n; k++) {
        /* Compute norm of column k below diagonal */
        double norm = 0.0;
        for (int64_t i = k; i < m; i++) {
            double val = A[i * lda + k];
            norm += val * val;
        }
        norm = sqrt(norm);

        if (norm < DBL_EPSILON) {
            tau[k] = 0.0;
            continue;
        }

        /* Compute Householder vector */
        double alpha = A[k * lda + k];
        double sign = (alpha >= 0.0) ? 1.0 : -1.0;
        double beta = alpha + sign * norm;

        /* Store Householder vector in column k */
        double inv_beta = 1.0 / beta;
        for (int64_t i = k + 1; i < m; i++) {
            A[i * lda + k] *= inv_beta;
        }

        /* Compute tau */
        tau[k] = beta / norm;

        /* Apply Householder reflection to remaining columns */
        for (int64_t j = k + 1; j < n; j++) {
            /* Compute dot product of Householder vector with column j */
            double dot = A[k * lda + j];
            for (int64_t i = k + 1; i < m; i++) {
                dot += A[i * lda + k] * A[i * lda + j];
            }
            dot *= tau[k];

            /* Update column j */
            A[k * lda + j] -= dot;
            for (int64_t i = k + 1; i < m; i++) {
                A[i * lda + j] -= A[i * lda + k] * dot;
            }
        }

        /* Store R diagonal element */
        A[k * lda + k] = -sign * norm;
    }

    return FC_OK;
}

/*
 * Cholesky decomposition
*/

int fc_mat_cholesky_decompose_f64(int64_t n, double* A, int64_t lda) {
    if (A == NULL) {
        return FC_ERR_INVALID_ARG;
    }
    if (n <= 0) {
        return FC_ERR_INVALID_ARG;
    }
    if (lda < n) {
        return FC_ERR_INVALID_ARG;
    }

    /* Cholesky-Banachiewicz algorithm */
    for (int64_t j = 0; j < n; j++) {
        /* Compute L[j,j] */
        double sum = A[j * lda + j];
        for (int64_t k = 0; k < j; k++) {
            double val = A[j * lda + k];
            sum -= val * val;
        }

        if (sum <= 0.0) {
            return FC_ERR_NOT_POSITIVE_DEF;
        }

        double ljj = sqrt(sum);
        A[j * lda + j] = ljj;

        /* Compute L[i,j] for i > j */
        double inv_ljj = 1.0 / ljj;
        for (int64_t i = j + 1; i < n; i++) {
            sum = A[i * lda + j];
            for (int64_t k = 0; k < j; k++) {
                sum -= A[i * lda + k] * A[j * lda + k];
            }
            A[i * lda + j] = sum * inv_ljj;
        }
    }

    return FC_OK;
}
