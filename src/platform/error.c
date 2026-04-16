/**
 * @file error.c
 * @brief Error handling implementation
 *
 * Provides error code to string mapping and error classification functionality.
 * Table is indexed by -code (enum values are 0, -1, -2, ...) for O(1) lookup.
 */

#include "error.h"

/*
 * Error code to string mapping table
 *
 * Indexed by -status. g_error_table[-FC_ERR_ASSERTION_FAILED] is the last entry.
 * Table order must match the enum ordering in error.h.
 */

typedef struct {
    const char* description;
    int severity;
    int fatal;
} fc_error_info_t;

static const fc_error_info_t g_error_table[] = {
    /* FC_OK                      */ { "Success",                         0, 0 },
    /* FC_ERR_INVALID_ARG         */ { "Invalid argument",                1, 0 },
    /* FC_ERR_DIMENSION_MISMATCH  */ { "Dimension mismatch",              2, 0 },
    /* FC_ERR_SINGULAR_MATRIX     */ { "Singular matrix (not invertible)",3, 0 },
    /* FC_ERR_NOT_POSITIVE_DEF    */ { "Matrix is not positive definite", 3, 0 },
    /* FC_ERR_CONVERGENCE         */ { "Iteration did not converge",      3, 0 },
    /* FC_ERR_WORKSPACE_TOO_SMALL */ { "Workspace too small",             2, 0 },
    /* FC_ERR_OVERFLOW            */ { "Numeric overflow",                3, 0 },
    /* FC_ERR_NAN_INPUT           */ { "Input contains NaN or Inf",       1, 0 },
    /* FC_ERR_ALIGNMENT           */ { "Memory alignment error",          2, 0 },
    /* FC_ERR_NOT_INITIALIZED     */ { "Library not initialized",         1, 0 },
    /* FC_ERR_BUFFER_TOO_SMALL    */ { "Output buffer too small",         2, 0 },
    /* FC_ERR_OUT_OF_MEMORY       */ { "Out of memory",                   4, 1 },
    /* FC_ERR_ILLEGAL_STATE       */ { "Illegal state",                   3, 0 },
    /* FC_ERR_NOT_IMPLEMENTED     */ { "Function not implemented",        2, 0 },
    /* FC_ERR_ASSERTION_FAILED    */ { "Assertion failed",                5, 1 },
};

#define FC_ERROR_TABLE_SIZE (sizeof(g_error_table) / sizeof(g_error_table[0]))

static int fc_status_valid(fc_status_t status) {
    return status <= FC_OK && status >= (fc_status_t)(-((int)FC_ERROR_TABLE_SIZE - 1));
}

/*
 * Public API implementation
*/

const char* fc_status_string(fc_status_t status) {
    if (fc_status_valid(status)) {
        return g_error_table[-status].description;
    }
    return "Unknown error";
}

int fc_is_fatal(fc_status_t status) {
    if (fc_status_valid(status)) {
        return g_error_table[-status].fatal;
    }
    return 0;
}

int fc_status_severity(fc_status_t status) {
    if (fc_status_valid(status)) {
        return g_error_table[-status].severity;
    }
    return 0;
}
