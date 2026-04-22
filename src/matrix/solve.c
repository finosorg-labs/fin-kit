/**
 * @file solve.c
 * @brief Linear system solvers and matrix inversion
 *
 * Provides implementations for solving linear systems and computing
 * matrix inverses using LU decomposition.
 */

#include <math.h>
#include <string.h>
#include <float.h>
#include <stdlib.h>
#include <matrix/matrix.h>
#include <platform/platform.h>
#include <platform/error.h>
#include <platform/mem_aligned.h>

/*
 * Linear system solver
*/

int fc_mat_solve_linear_f64(int64_t n, int64_t nrhs,
                            double* A, int64_t lda,
                            double* B, int64_t ldb) {
    if (A == NULL || B == NULL) {
        return FC_ERR_INVALID_ARG;
    }
    if (n <= 0 || nrhs <= 0) {
        return FC_ERR_INVALID_ARG;
    }
    if (lda < n || ldb < nrhs) {
        return FC_ERR_INVALID_ARG;
    }

    /* Allocate pivot array */
    int64_t* ipiv = (int64_t*)fc_aligned_alloc(n * sizeof(int64_t), 64);
    if (ipiv == NULL) {
        return FC_ERR_OUT_OF_MEMORY;
    }

    /* Compute LU decomposition */
    int status = fc_mat_lu_decompose_f64(n, A, lda, ipiv);
    if (status != FC_OK) {
        fc_aligned_free(ipiv);
        return status;
    }

    /* Solve for each right-hand side */
    for (int64_t k = 0; k < nrhs; k++) {
        double* b = B + k;

        /* Apply row permutation to b */
        double* temp = (double*)fc_aligned_alloc(n * sizeof(double), 64);
        if (temp == NULL) {
            fc_aligned_free(ipiv);
            return FC_ERR_OUT_OF_MEMORY;
        }

        for (int64_t i = 0; i < n; i++) {
            temp[i] = b[ipiv[i] * ldb];
        }
        for (int64_t i = 0; i < n; i++) {
            b[i * ldb] = temp[i];
        }
        fc_aligned_free(temp);

        /* Forward substitution: solve L * y = Pb */
        for (int64_t i = 0; i < n; i++) {
            double sum = b[i * ldb];
            for (int64_t j = 0; j < i; j++) {
                sum -= A[i * lda + j] * b[j * ldb];
            }
            b[i * ldb] = sum;
        }

        /* Backward substitution: solve U * x = y */
        for (int64_t i = n - 1; i >= 0; i--) {
            double sum = b[i * ldb];
            for (int64_t j = i + 1; j < n; j++) {
                sum -= A[i * lda + j] * b[j * ldb];
            }
            b[i * ldb] = sum / A[i * lda + i];
        }
    }

    fc_aligned_free(ipiv);
    return FC_OK;
}

/*
 * Matrix inversion
*/

int fc_mat_inverse_f64(int64_t n, double* A, int64_t lda) {
    if (A == NULL) {
        return FC_ERR_INVALID_ARG;
    }
    if (n <= 0) {
        return FC_ERR_INVALID_ARG;
    }
    if (lda < n) {
        return FC_ERR_INVALID_ARG;
    }

    /* Allocate workspace for identity matrix and pivot array */
    double* I = (double*)fc_aligned_alloc(n * n * sizeof(double), 64);
    int64_t* ipiv = (int64_t*)fc_aligned_alloc(n * sizeof(int64_t), 64);

    if (I == NULL || ipiv == NULL) {
        if (I) fc_aligned_free(I);
        if (ipiv) fc_aligned_free(ipiv);
        return FC_ERR_OUT_OF_MEMORY;
    }

    /* Initialize identity matrix */
    memset(I, 0, n * n * sizeof(double));
    for (int64_t i = 0; i < n; i++) {
        I[i * n + i] = 1.0;
    }

    /* Compute LU decomposition */
    int status = fc_mat_lu_decompose_f64(n, A, lda, ipiv);
    if (status != FC_OK) {
        fc_aligned_free(I);
        fc_aligned_free(ipiv);
        return status;
    }

    /* Allocate temporary array for permutation */
    double* temp = (double*)fc_aligned_alloc(n * sizeof(double), 64);
    if (temp == NULL) {
        fc_aligned_free(I);
        fc_aligned_free(ipiv);
        return FC_ERR_OUT_OF_MEMORY;
    }

    /* Solve A * X = I for each column of I */
    for (int64_t k = 0; k < n; k++) {
        double* b = I + k;

        /* Apply row permutation */
        for (int64_t i = 0; i < n; i++) {
            temp[i] = b[ipiv[i] * n];
        }
        for (int64_t i = 0; i < n; i++) {
            b[i * n] = temp[i];
        }

        /* Forward substitution */
        for (int64_t i = 0; i < n; i++) {
            double sum = b[i * n];
            for (int64_t j = 0; j < i; j++) {
                sum -= A[i * lda + j] * b[j * n];
            }
            b[i * n] = sum;
        }

        /* Backward substitution */
        for (int64_t i = n - 1; i >= 0; i--) {
            double sum = b[i * n];
            for (int64_t j = i + 1; j < n; j++) {
                sum -= A[i * lda + j] * b[j * n];
            }
            b[i * n] = sum / A[i * lda + i];
        }
    }

    /* Copy result back to A */
    for (int64_t i = 0; i < n; i++) {
        for (int64_t j = 0; j < n; j++) {
            A[i * lda + j] = I[i * n + j];
        }
    }

    fc_aligned_free(temp);
    fc_aligned_free(I);
    fc_aligned_free(ipiv);
    return FC_OK;
}
