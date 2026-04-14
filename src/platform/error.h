/**
 * @file error.h
 * @brief Error code definitions and error handling
 *
 * Defines the unified error code system for fin-kit library, providing
 * thread-safe error description queries.
 */

#ifndef FC_ERROR_H
#define FC_ERROR_H

#include "platform.h"

FC_BEGIN_DECLS

/**
 * @brief Library function return status codes
 *
 * All public APIs use this enum for return values, following the
 * "fail-fast" principle.
 * Negative values indicate errors, positive values indicate warnings
 * (currently unused), and 0 indicates success.
 */
typedef enum {
    FC_OK                         =  0,  /**< Success */
    FC_ERR_INVALID_ARG            = -1,  /**< Invalid argument (null pointer, negative length, etc.) */
    FC_ERR_DIMENSION_MISMATCH     = -2,  /**< Matrix/vector dimension mismatch */
    FC_ERR_SINGULAR_MATRIX       = -3,  /**< Singular matrix (not invertible) */
    FC_ERR_NOT_POSITIVE_DEF      = -4,  /**< Matrix not positive definite (Cholesky failure) */
    FC_ERR_CONVERGENCE           = -5,  /**< Iterative algorithm did not converge */
    FC_ERR_WORKSPACE_TOO_SMALL    = -6,  /**< Insufficient workspace memory */
    FC_ERR_OVERFLOW              = -7,  /**< Numeric overflow */
    FC_ERR_NAN_INPUT             = -8,  /**< Input contains NaN */
    FC_ERR_ALIGNMENT             = -9,  /**< Memory alignment error */
    FC_ERR_NOT_INITIALIZED       = -10, /**< Library not initialized */
    FC_ERR_BUFFER_TOO_SMALL      = -11, /**< Output buffer too small */
    FC_ERR_OUT_OF_MEMORY         = -12, /**< Memory allocation failed */
    FC_ERR_ILLEGAL_STATE         = -13, /**< Illegal state */
    FC_ERR_NOT_IMPLEMENTED       = -14, /**< Function not implemented */
    FC_ERR_ASSERTION_FAILED      = -15, /**< Assertion failed (debug mode only) */
} fc_status_t;

/**
 * @brief Get description string for error code
 *
 * Returns a static constant string. Thread-safe.
 *
 * @param status Error code
 *
 * @return Error description string (English)
 */
const char* fc_status_string(fc_status_t status);

/**
 * @brief Check if status code indicates success
 *
 * Equivalent to (status == FC_OK), provided as a convenience function.
 * For performance-critical paths, prefer direct comparison: if (status == FC_OK).
 *
 * @param status Error code
 *
 * @return 1 if success, 0 if failure
 */
FC_INLINE int fc_is_ok(fc_status_t status) {
    return status == FC_OK;
}

/**
 * @brief Check if this is a fatal error
 *
 * Fatal errors indicate the program cannot continue and requires
 * termination or state reset.
 *
 * @param status Error code
 *
 * @return 1 if fatal error, 0 if recoverable error
 */
int fc_is_fatal(fc_status_t status);

/**
 * @brief Get error severity level
 *
 * @param status Error code
 *
 * @return Severity value (higher = more severe)
 */
int fc_status_severity(fc_status_t status);

FC_END_DECLS

#endif /* FC_ERROR_H */
