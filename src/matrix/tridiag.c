/**
 * @file tridiag.c
 * @brief Tridiagonal matrix solver using Thomas algorithm
 *
 * Provides efficient O(n) solver for tridiagonal linear systems,
 * commonly used in finite difference methods for PDEs.
 */

#include <math.h>
#include <string.h>
#include <float.h>
#include <matrix/tridiag.h>
#include <platform.h>
#include <error.h>
#include <mem_aligned.h>

/**
 * @brief Solve tridiagonal system using Thomas algorithm
 *
 * Solves the system: A*x = d
 * where A is a tridiagonal matrix with:
 *   - lower diagonal: a[1..n-1]
 *   - main diagonal:  b[0..n-1]
 *   - upper diagonal: c[0..n-2]
 *
 * The Thomas algorithm is a specialized form of Gaussian elimination
 * that takes advantage of the tridiagonal structure, achieving O(n)
 * time complexity instead of O(n^3).
 *
 * Algorithm:
 * 1. Forward elimination: modify c and d
 * 2. Back substitution: solve for x
 *
 * Time complexity: O(n)
 * Space complexity: O(n) for workspace
 * Thread safety: Thread-safe (no shared state)
 *
 * @param[in]    n     Size of the system (must be > 1)
 * @param[in]    a     Lower diagonal (length n-1), a[0] is ignored
 * @param[in]    b     Main diagonal (length n)
 * @param[in]    c     Upper diagonal (length n-1), c[n-1] is ignored
 * @param[in]    d     Right-hand side vector (length n)
 * @param[out]   x     Solution vector (length n)
 *
 * @return FC_OK on success, FC_ERR_INVALID_ARG if inputs invalid,
 *         FC_ERR_SINGULAR_MATRIX if matrix is singular
 *
 * @note The input arrays a, b, c, d are not modified
 * @note The algorithm requires that the matrix is diagonally dominant
 *       or positive definite for numerical stability
 */
int fc_mat_tridiag_solve_f64(int64_t n,
                              const double* a, const double* b, const double* c,
                              const double* d, double* x) {
    if (a == NULL || b == NULL || c == NULL || d == NULL || x == NULL) {
        return FC_ERR_INVALID_ARG;
    }
    if (n <= 1) {
        return FC_ERR_INVALID_ARG;
    }

    /* Allocate workspace for modified coefficients */
    double* c_prime = (double*)fc_aligned_alloc(n * sizeof(double), 64);
    double* d_prime = (double*)fc_aligned_alloc(n * sizeof(double), 64);

    if (c_prime == NULL || d_prime == NULL) {
        if (c_prime) fc_aligned_free(c_prime);
        if (d_prime) fc_aligned_free(d_prime);
        return FC_ERR_OUT_OF_MEMORY;
    }

    /* Forward elimination */
    /* First row */
    if (fabs(b[0]) < DBL_EPSILON) {
        fc_aligned_free(c_prime);
        fc_aligned_free(d_prime);
        return FC_ERR_SINGULAR_MATRIX;
    }

    c_prime[0] = c[0] / b[0];
    d_prime[0] = d[0] / b[0];

    /* Remaining rows */
    for (int64_t i = 1; i < n; i++) {
        double denom = b[i] - a[i] * c_prime[i-1];

        if (fabs(denom) < DBL_EPSILON) {
            fc_aligned_free(c_prime);
            fc_aligned_free(d_prime);
            return FC_ERR_SINGULAR_MATRIX;
        }

        if (i < n - 1) {
            c_prime[i] = c[i] / denom;
        }
        d_prime[i] = (d[i] - a[i] * d_prime[i-1]) / denom;
    }

    /* Back substitution */
    x[n-1] = d_prime[n-1];
    for (int64_t i = n - 2; i >= 0; i--) {
        x[i] = d_prime[i] - c_prime[i] * x[i+1];
    }

    fc_aligned_free(c_prime);
    fc_aligned_free(d_prime);
    return FC_OK;
}

/**
 * @brief Solve tridiagonal system with multiple right-hand sides
 *
 * Solves: A*X = D
 * where A is tridiagonal (n×n) and D is n×nrhs
 *
 * This is more efficient than calling the single RHS version multiple times
 * because the forward elimination only needs to be done once.
 *
 * @param[in]    n     Size of the system (must be > 1)
 * @param[in]    nrhs  Number of right-hand sides (must be > 0)
 * @param[in]    a     Lower diagonal (length n-1)
 * @param[in]    b     Main diagonal (length n)
 * @param[in]    c     Upper diagonal (length n-1)
 * @param[in]    D     Right-hand side matrix (n×nrhs, row-major)
 * @param[in]    ldd   Leading dimension of D (stride, typically nrhs)
 * @param[out]   X     Solution matrix (n×nrhs, row-major)
 * @param[in]    ldx   Leading dimension of X (stride, typically nrhs)
 *
 * @return FC_OK on success, error code otherwise
 */
int fc_mat_tridiag_solve_multi_f64(int64_t n, int64_t nrhs,
                                    const double* a, const double* b, const double* c,
                                    const double* D, int64_t ldd,
                                    double* X, int64_t ldx) {
    if (a == NULL || b == NULL || c == NULL || D == NULL || X == NULL) {
        return FC_ERR_INVALID_ARG;
    }
    if (n <= 1 || nrhs <= 0) {
        return FC_ERR_INVALID_ARG;
    }
    if (ldd < nrhs || ldx < nrhs) {
        return FC_ERR_INVALID_ARG;
    }

    /* Allocate workspace */
    double* c_prime = (double*)fc_aligned_alloc(n * sizeof(double), 64);

    if (c_prime == NULL) {
        return FC_ERR_OUT_OF_MEMORY;
    }

    /* Forward elimination (only once for all RHS) */
    if (fabs(b[0]) < DBL_EPSILON) {
        fc_aligned_free(c_prime);
        return FC_ERR_SINGULAR_MATRIX;
    }

    c_prime[0] = c[0] / b[0];

    for (int64_t i = 1; i < n; i++) {
        double denom = b[i] - a[i] * c_prime[i-1];

        if (fabs(denom) < DBL_EPSILON) {
            fc_aligned_free(c_prime);
            return FC_ERR_SINGULAR_MATRIX;
        }

        if (i < n - 1) {
            c_prime[i] = c[i] / denom;
        }
    }

    /* Solve for each right-hand side */
    for (int64_t k = 0; k < nrhs; k++) {
        /* Forward substitution for this RHS */
        double d_prime = D[0 * ldd + k] / b[0];
        X[0 * ldx + k] = d_prime;

        for (int64_t i = 1; i < n; i++) {
            double denom = b[i] - a[i] * c_prime[i-1];
            d_prime = (D[i * ldd + k] - a[i] * d_prime) / denom;
            X[i * ldx + k] = d_prime;
        }

        /* Back substitution for this RHS */
        for (int64_t i = n - 2; i >= 0; i--) {
            X[i * ldx + k] = X[i * ldx + k] - c_prime[i] * X[(i+1) * ldx + k];
        }
    }

    fc_aligned_free(c_prime);
    return FC_OK;
}
