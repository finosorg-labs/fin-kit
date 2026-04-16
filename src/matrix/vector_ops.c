/**
 * @file vector_ops.c
 * @brief Vector operations implementation (dot product, norms, distances)
 *
 * Provides scalar baseline implementations with numerical stability.
 */

#include <math.h>
#include <float.h>
#include "../platform/platform.h"
#include "../platform/error.h"

/*
 * Input validation helpers
*/

static FC_INLINE int validate_vector_inputs(const double* x, const double* y, int n) {
    if (x == NULL || y == NULL) {
        return FC_ERR_INVALID_ARG;
    }
    if (n <= 0) {
        return FC_ERR_INVALID_ARG;
    }
    return FC_OK;
}

static FC_INLINE int validate_single_vector(const double* x, int n) {
    if (x == NULL) {
        return FC_ERR_INVALID_ARG;
    }
    if (n <= 0) {
        return FC_ERR_INVALID_ARG;
    }
    return FC_OK;
}

/*
 * Dot product
*/

/**
 * @brief Compute dot product using Kahan summation for numerical stability
 *
 * Uses compensated summation to reduce floating-point rounding errors.
 * This is critical for financial applications where precision matters.
 */
int fc_vec_dot_f64(const double* x, const double* y, int n, double* result) {
    if (result == NULL) {
        return FC_ERR_INVALID_ARG;
    }

    int status = validate_vector_inputs(x, y, n);
    if (status != FC_OK) {
        return status;
    }

    /* Kahan summation algorithm for numerical stability */
    double sum = 0.0;
    double c = 0.0;  /* Running compensation for lost low-order bits */

    for (int i = 0; i < n; i++) {
        double product = x[i] * y[i];
        double y_val = product - c;
        double t = sum + y_val;
        c = (t - sum) - y_val;
        sum = t;
    }

    *result = sum;
    return FC_OK;
}

/*
 * L2 norm (Euclidean norm)
*/

/**
 * @brief Compute L2 norm with overflow/underflow protection
 *
 * Uses scaling technique to avoid overflow for large values and
 * underflow for small values, following BLAS dnrm2 approach.
 */
int fc_vec_norm_l2_f64(const double* x, int n, double* result) {
    if (result == NULL) {
        return FC_ERR_INVALID_ARG;
    }

    int status = validate_single_vector(x, n);
    if (status != FC_OK) {
        return status;
    }

    if (n == 1) {
        *result = fabs(x[0]);
        return FC_OK;
    }

    /* Scale-based algorithm to prevent overflow/underflow */
    double scale = 0.0;
    double ssq = 1.0;  /* Sum of squares */

    for (int i = 0; i < n; i++) {
        double abs_val = fabs(x[i]);

        if (abs_val > 0.0) {
            if (scale < abs_val) {
                double ratio = scale / abs_val;
                ssq = 1.0 + ssq * ratio * ratio;
                scale = abs_val;
            } else {
                double ratio = abs_val / scale;
                ssq += ratio * ratio;
            }
        }
    }

    *result = scale * sqrt(ssq);
    return FC_OK;
}

/*
 * L1 norm (Manhattan norm)
*/

/**
 * @brief Compute L1 norm using Kahan summation
 */
int fc_vec_norm_l1_f64(const double* x, int n, double* result) {
    if (result == NULL) {
        return FC_ERR_INVALID_ARG;
    }

    int status = validate_single_vector(x, n);
    if (status != FC_OK) {
        return status;
    }

    /* Kahan summation for numerical stability */
    double sum = 0.0;
    double c = 0.0;

    for (int i = 0; i < n; i++) {
        double abs_val = fabs(x[i]);
        double y_val = abs_val - c;
        double t = sum + y_val;
        c = (t - sum) - y_val;
        sum = t;
    }

    *result = sum;
    return FC_OK;
}

/*
 * Euclidean distance
*/

/**
 * @brief Compute Euclidean distance with overflow protection
 *
 * Computes ||x - y||_2 using the same scaling technique as L2 norm.
 */
int fc_vec_distance_l2_f64(const double* x, const double* y, int n, double* result) {
    if (result == NULL) {
        return FC_ERR_INVALID_ARG;
    }

    int status = validate_vector_inputs(x, y, n);
    if (status != FC_OK) {
        return status;
    }

    if (n == 1) {
        *result = fabs(x[0] - y[0]);
        return FC_OK;
    }

    /* Scale-based algorithm */
    double scale = 0.0;
    double ssq = 1.0;

    for (int i = 0; i < n; i++) {
        double diff = x[i] - y[i];
        double abs_diff = fabs(diff);

        if (abs_diff > 0.0) {
            if (scale < abs_diff) {
                double ratio = scale / abs_diff;
                ssq = 1.0 + ssq * ratio * ratio;
                scale = abs_diff;
            } else {
                double ratio = abs_diff / scale;
                ssq += ratio * ratio;
            }
        }
    }

    *result = scale * sqrt(ssq);
    return FC_OK;
}

/*
 * Manhattan distance
*/

/**
 * @brief Compute Manhattan distance using Kahan summation
 */
int fc_vec_distance_l1_f64(const double* x, const double* y, int n, double* result) {
    if (result == NULL) {
        return FC_ERR_INVALID_ARG;
    }

    int status = validate_vector_inputs(x, y, n);
    if (status != FC_OK) {
        return status;
    }

    /* Kahan summation */
    double sum = 0.0;
    double c = 0.0;

    for (int i = 0; i < n; i++) {
        double abs_diff = fabs(x[i] - y[i]);
        double y_val = abs_diff - c;
        double t = sum + y_val;
        c = (t - sum) - y_val;
        sum = t;
    }

    *result = sum;
    return FC_OK;
}
