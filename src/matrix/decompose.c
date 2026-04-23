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
#include <platform/mem_aligned.h>

/*
 * Helper functions
*/

/* Partition function for quicksort with eigenvector reordering */
static int64_t partition_eigenvalues(double* w, double* Q, int64_t ldq, int64_t n,
                                     int64_t low, int64_t high, int ascending) {
    double pivot = w[high];
    int64_t i = low - 1;

    for (int64_t j = low; j < high; j++) {
        int should_swap = ascending ? (w[j] < pivot) : (w[j] > pivot);
        if (should_swap) {
            i++;
            /* Swap eigenvalues */
            double temp = w[i];
            w[i] = w[j];
            w[j] = temp;

            /* Swap eigenvector columns */
            for (int64_t k = 0; k < n; k++) {
                temp = Q[k * ldq + i];
                Q[k * ldq + i] = Q[k * ldq + j];
                Q[k * ldq + j] = temp;
            }
        }
    }

    /* Place pivot in correct position */
    i++;
    double temp = w[i];
    w[i] = w[high];
    w[high] = temp;

    for (int64_t k = 0; k < n; k++) {
        temp = Q[k * ldq + i];
        Q[k * ldq + i] = Q[k * ldq + high];
        Q[k * ldq + high] = temp;
    }

    return i;
}

/* Quicksort for eigenvalues with eigenvector reordering */
static void quicksort_eigenvalues(double* w, double* Q, int64_t ldq, int64_t n,
                                  int64_t low, int64_t high, int ascending) {
    if (low < high) {
        int64_t pi = partition_eigenvalues(w, Q, ldq, n, low, high, ascending);
        quicksort_eigenvalues(w, Q, ldq, n, low, pi - 1, ascending);
        quicksort_eigenvalues(w, Q, ldq, n, pi + 1, high, ascending);
    }
}

/* Sort eigenvalues and eigenvectors (ascending or descending) */
static void sort_eigenvalues(double* w, double* Q, int64_t ldq, int64_t n, int ascending) {
    const int64_t QUICKSORT_THRESHOLD = 50;

    if (n <= QUICKSORT_THRESHOLD) {
        /* Use selection sort for small arrays */
        for (int64_t i = 0; i < n - 1; i++) {
            int64_t k = i;
            double p = w[i];

            for (int64_t j = i + 1; j < n; j++) {
                int should_select = ascending ? (w[j] < p) : (w[j] > p);
                if (should_select) {
                    k = j;
                    p = w[j];
                }
            }

            if (k != i) {
                w[k] = w[i];
                w[i] = p;

                for (int64_t j = 0; j < n; j++) {
                    double temp = Q[j * ldq + i];
                    Q[j * ldq + i] = Q[j * ldq + k];
                    Q[j * ldq + k] = temp;
                }
            }
        }
    } else {
        /* Use quicksort for large arrays */
        quicksort_eigenvalues(w, Q, ldq, n, 0, n - 1, ascending);
    }
}

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

/*
 * Singular Value Decomposition
 */

int fc_mat_svd_f64(int64_t m, int64_t n, double* A, int64_t lda,
                   double* s, double* U, int64_t ldu, double* VT, int64_t ldvt) {
    if (A == NULL || s == NULL) {
        return FC_ERR_INVALID_ARG;
    }
    if (m <= 0 || n <= 0) {
        return FC_ERR_INVALID_ARG;
    }
    if (lda < n) {
        return FC_ERR_INVALID_ARG;
    }
    if (U != NULL && ldu < m) {
        return FC_ERR_INVALID_ARG;
    }
    if (VT != NULL && ldvt < n) {
        return FC_ERR_INVALID_ARG;
    }

    /*
     * SVD via eigenvalue decomposition:
     * For m >= n: compute A^T*A, get V and Λ, then U = A*V*Σ^(-1)
     * For m < n: compute A*A^T, get U and Λ, then V = A^T*U*Σ^(-1)
     */

    if (m >= n) {
        /* Compute A^T*A */
        double* ATA = fc_aligned_alloc_double(n * n);
        if (ATA == NULL) {
            return FC_ERR_OUT_OF_MEMORY;
        }

        for (int64_t i = 0; i < n; i++) {
            for (int64_t j = 0; j < n; j++) {
                double sum = 0.0;
                for (int64_t k = 0; k < m; k++) {
                    sum += A[k * lda + i] * A[k * lda + j];
                }
                ATA[i * n + j] = sum;
            }
        }

        /* Compute eigenvalue decomposition of A^T*A */
        double* eigenvalues = fc_aligned_alloc_double(n);
        double* V = fc_aligned_alloc_double(n * n);
        if (eigenvalues == NULL || V == NULL) {
            fc_aligned_free(ATA);
            fc_aligned_free(eigenvalues);
            fc_aligned_free(V);
            return FC_ERR_OUT_OF_MEMORY;
        }

        int ret = fc_mat_eig_sym_f64(n, ATA, n, eigenvalues, V, n);
        if (ret != FC_OK) {
            fc_aligned_free(ATA);
            fc_aligned_free(eigenvalues);
            fc_aligned_free(V);
            return ret;
        }

        /* Compute singular values from eigenvalues */
        for (int64_t i = 0; i < n; i++) {
            s[i] = (eigenvalues[i] > 0.0) ? sqrt(eigenvalues[i]) : 0.0;
        }

        /* Sort singular values in descending order and reorder V columns */
        sort_eigenvalues(s, V, n, n, 0);

        /* Compute VT if requested (transpose of V) */
        if (VT != NULL) {
            for (int64_t i = 0; i < n; i++) {
                for (int64_t j = 0; j < n; j++) {
                    VT[i * ldvt + j] = V[j * n + i];
                }
            }
        }

        /* Compute U = A*V*Σ^(-1) if requested */
        if (U != NULL) {
            for (int64_t i = 0; i < m; i++) {
                for (int64_t j = 0; j < n; j++) {
                    double sum = 0.0;
                    for (int64_t k = 0; k < n; k++) {
                        sum += A[i * lda + k] * V[k * n + j];
                    }
                    /* Normalize by singular value */
                    if (s[j] > DBL_EPSILON) {
                        U[i * ldu + j] = sum / s[j];
                    } else {
                        U[i * ldu + j] = 0.0;
                    }
                }
            }
        }

        fc_aligned_free(ATA);
        fc_aligned_free(eigenvalues);
        fc_aligned_free(V);

    } else {
        /* m < n: compute A*A^T */
        double* AAT = fc_aligned_alloc_double(m * m);
        if (AAT == NULL) {
            return FC_ERR_OUT_OF_MEMORY;
        }

        for (int64_t i = 0; i < m; i++) {
            for (int64_t j = 0; j < m; j++) {
                double sum = 0.0;
                for (int64_t k = 0; k < n; k++) {
                    sum += A[i * lda + k] * A[j * lda + k];
                }
                AAT[i * m + j] = sum;
            }
        }

        /* Compute eigenvalue decomposition of A*A^T */
        double* eigenvalues = fc_aligned_alloc_double(m);
        double* U_temp = fc_aligned_alloc_double(m * m);
        if (eigenvalues == NULL || U_temp == NULL) {
            fc_aligned_free(AAT);
            fc_aligned_free(eigenvalues);
            fc_aligned_free(U_temp);
            return FC_ERR_OUT_OF_MEMORY;
        }

        int ret = fc_mat_eig_sym_f64(m, AAT, m, eigenvalues, U_temp, m);
        if (ret != FC_OK) {
            fc_aligned_free(AAT);
            fc_aligned_free(eigenvalues);
            fc_aligned_free(U_temp);
            return ret;
        }

        /* Compute singular values from eigenvalues */
        for (int64_t i = 0; i < m; i++) {
            s[i] = (eigenvalues[i] > 0.0) ? sqrt(eigenvalues[i]) : 0.0;
        }

        /* Sort singular values in descending order and reorder U columns */
        sort_eigenvalues(s, U_temp, m, m, 0);

        /* Copy U if requested */
        if (U != NULL) {
            for (int64_t i = 0; i < m; i++) {
                for (int64_t j = 0; j < m; j++) {
                    U[i * ldu + j] = U_temp[i * m + j];
                }
            }
        }

        /* Compute VT = Σ^(-1)*U^T*A if requested */
        if (VT != NULL) {
            for (int64_t i = 0; i < m; i++) {
                for (int64_t j = 0; j < n; j++) {
                    double sum = 0.0;
                    for (int64_t k = 0; k < m; k++) {
                        sum += U_temp[k * m + i] * A[k * lda + j];
                    }
                    /* Normalize by singular value */
                    if (s[i] > DBL_EPSILON) {
                        VT[i * ldvt + j] = sum / s[i];
                    } else {
                        VT[i * ldvt + j] = 0.0;
                    }
                }
            }
        }

        fc_aligned_free(AAT);
        fc_aligned_free(eigenvalues);
        fc_aligned_free(U_temp);
    }

    return FC_OK;
}

/*
 * Eigenvalue decomposition for symmetric matrices
 */

int fc_mat_eig_sym_f64(int64_t n, double* A, int64_t lda,
                       double* w, double* Q, int64_t ldq) {
    if (A == NULL || w == NULL || Q == NULL) {
        return FC_ERR_INVALID_ARG;
    }
    if (n <= 0) {
        return FC_ERR_INVALID_ARG;
    }
    if (lda < n || ldq < n) {
        return FC_ERR_INVALID_ARG;
    }

    /* Initialize Q as identity matrix */
    memset(Q, 0, n * ldq * sizeof(double));
    for (int64_t i = 0; i < n; i++) {
        Q[i * ldq + i] = 1.0;
    }

    /* Jacobi iteration for symmetric eigenvalue decomposition */
    const int max_iter = 50 * n * n;
    const double epsilon = 1e-15;

    for (int iter = 0; iter < max_iter; iter++) {
        /* Find largest off-diagonal element */
        double max_val = 0.0;
        int64_t p = 0, q = 1;

        for (int64_t i = 0; i < n; i++) {
            for (int64_t j = i + 1; j < n; j++) {
                double val = fabs(A[i * lda + j]);
                if (val > max_val) {
                    max_val = val;
                    p = i;
                    q = j;
                }
            }
        }

        /* Check convergence */
        if (max_val < epsilon) {
            break;
        }

        /* Compute Jacobi rotation */
        double app = A[p * lda + p];
        double aqq = A[q * lda + q];
        double apq = A[p * lda + q];

        double theta = 0.5 * atan2(2.0 * apq, aqq - app);
        double c = cos(theta);
        double s = sin(theta);

        /* Apply rotation to A: A = J^T * A * J */
        /* Update rows p and q */
        for (int64_t j = 0; j < n; j++) {
            if (j != p && j != q) {
                double ajp = A[j * lda + p];
                double ajq = A[j * lda + q];
                A[j * lda + p] = c * ajp - s * ajq;
                A[j * lda + q] = s * ajp + c * ajq;

                /* Maintain symmetry */
                A[p * lda + j] = A[j * lda + p];
                A[q * lda + j] = A[j * lda + q];
            }
        }

        /* Update diagonal elements */
        double new_app = c * c * app + s * s * aqq - 2.0 * s * c * apq;
        double new_aqq = s * s * app + c * c * aqq + 2.0 * s * c * apq;
        A[p * lda + p] = new_app;
        A[q * lda + q] = new_aqq;
        A[p * lda + q] = 0.0;
        A[q * lda + p] = 0.0;

        /* Accumulate rotation in Q: Q = Q * J */
        for (int64_t i = 0; i < n; i++) {
            double qip = Q[i * ldq + p];
            double qiq = Q[i * ldq + q];
            Q[i * ldq + p] = c * qip - s * qiq;
            Q[i * ldq + q] = s * qip + c * qiq;
        }
    }

    /* Extract eigenvalues from diagonal */
    for (int64_t i = 0; i < n; i++) {
        w[i] = A[i * lda + i];
    }

    /* Sort eigenvalues and eigenvectors in ascending order */
    sort_eigenvalues(w, Q, ldq, n, 1);

    return FC_OK;
}
