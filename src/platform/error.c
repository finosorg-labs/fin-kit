/**
 * @file error.c
 * @brief Error handling implementation
 *
 * Provides error code to string mapping and error classification functionality.
 */

#include "error.h"
#include <string.h>

/* ============================================================================
 * Error code to string mapping table
 * ============================================================================ */

typedef struct {
    fc_status_t code;
    const char* description;
    int severity;
    int fatal;
} fc_error_info_t;

static const fc_error_info_t g_error_table[] = {
    { FC_OK,                       "Success",                              0, 0 },
    { FC_ERR_INVALID_ARG,          "Invalid argument",                     1, 0 },
    { FC_ERR_ALIGNMENT,           "Memory alignment error",               2, 0 },
    { FC_ERR_NAN_INPUT,           "Input contains NaN or Inf",           1, 0 },
    { FC_ERR_DIMENSION_MISMATCH,  "Dimension mismatch",                   2, 0 },
    { FC_ERR_ILLEGAL_STATE,       "Illegal state",                        3, 0 },
    { FC_ERR_SINGULAR_MATRIX,     "Singular matrix (not invertible)",     3, 0 },
    { FC_ERR_NOT_POSITIVE_DEF,    "Matrix is not positive definite",      3, 0 },
    { FC_ERR_OVERFLOW,            "Numeric overflow",                     3, 0 },
    { FC_ERR_CONVERGENCE,         "Iteration did not converge",           3, 0 },
    { FC_ERR_WORKSPACE_TOO_SMALL, "Workspace too small",                  2, 0 },
    { FC_ERR_BUFFER_TOO_SMALL,    "Output buffer too small",              2, 0 },
    { FC_ERR_OUT_OF_MEMORY,       "Out of memory",                        4, 1 },
    { FC_ERR_NOT_INITIALIZED,     "Library not initialized",               1, 0 },
    { FC_ERR_NOT_IMPLEMENTED,     "Function not implemented",             2, 0 },
    { FC_ERR_ASSERTION_FAILED,    "Assertion failed",                     5, 1 },
};

#define FC_ERROR_TABLE_SIZE (sizeof(g_error_table) / sizeof(g_error_table[0]))

/* ============================================================================
 * Public API implementation
 * ============================================================================ */

const char* fc_status_string(fc_status_t status) {
    for (size_t i = 0; i < FC_ERROR_TABLE_SIZE; i++) {
        if (g_error_table[i].code == status) {
            return g_error_table[i].description;
        }
    }
    return "Unknown error";
}

int fc_is_ok(fc_status_t status) {
    return status == FC_OK;
}

int fc_is_fatal(fc_status_t status) {
    for (size_t i = 0; i < FC_ERROR_TABLE_SIZE; i++) {
        if (g_error_table[i].code == status) {
            return g_error_table[i].fatal;
        }
    }
    return 0;
}

int fc_status_severity(fc_status_t status) {
    for (size_t i = 0; i < FC_ERROR_TABLE_SIZE; i++) {
        if (g_error_table[i].code == status) {
            return g_error_table[i].severity;
        }
    }
    return 0;
}
