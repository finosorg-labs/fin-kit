/**
 * @file test_matrix.c
 * @brief Unit tests for matrix operations
 *
 * Tests vector operations and GEMM
 */

#ifndef FC_ENABLE_INTERNAL_TESTS
#define FC_ENABLE_INTERNAL_TESTS
#endif

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "test_framework.h"
#include <matrix/matrix.h>
#include <platform/error.h>
#include <platform/simd_detect.h>
#include <matrix/matrix_internal.h>

/* Test tolerance for floating-point comparisons */
#define TEST_EPSILON 1e-12
#define TEST_EPSILON_RELAXED 1e-10

/*
 * Helper functions
*/

static int double_equals(double a, double b, double epsilon) {
    return fabs(a - b) < epsilon;
}

/*
 * Vector operations tests
*/


TEST(test_cholesky_decompose_basic) {
    /* Symmetric positive definite matrix */
    double A[] = {
        4.0, 12.0, -16.0,
        12.0, 37.0, -43.0,
        -16.0, -43.0, 98.0
    };

    int status = fc_mat_cholesky_decompose_f64(3, A, 3);
    ASSERT_EQ(status, FC_OK);

    /* Verify L * L^T = original A by checking diagonal elements */
    ASSERT_TRUE(A[0] > 0.0);
    ASSERT_TRUE(A[4] > 0.0);
    ASSERT_TRUE(A[8] > 0.0);
}

TEST(test_cholesky_decompose_identity) {
    double A[] = {
        1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0
    };

    int status = fc_mat_cholesky_decompose_f64(3, A, 3);
    ASSERT_EQ(status, FC_OK);

    /* Identity should remain identity */
    ASSERT_TRUE(double_equals(A[0], 1.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(A[4], 1.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(A[8], 1.0, TEST_EPSILON));
}

TEST(test_cholesky_decompose_invalid_args) {
    double A[9];

    ASSERT_EQ(fc_mat_cholesky_decompose_f64(3, NULL, 3), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_cholesky_decompose_f64(0, A, 3), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_cholesky_decompose_f64(-1, A, 3), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_cholesky_decompose_f64(3, A, 2), FC_ERR_INVALID_ARG);
}

TEST(test_cholesky_decompose_not_positive_def) {
    /* Not positive definite (negative eigenvalue) */
    double A[] = {
        1.0, 2.0, 3.0,
        2.0, 1.0, 4.0,
        3.0, 4.0, 1.0
    };

    int status = fc_mat_cholesky_decompose_f64(3, A, 3);
    ASSERT_EQ(status, FC_ERR_NOT_POSITIVE_DEF);
}

TEST(test_lu_decompose_basic) {
    /* Test 3x3 matrix */
    double A[] = {
        2.0, 1.0, 1.0,
        4.0, 3.0, 3.0,
        8.0, 7.0, 9.0
    };
    int64_t ipiv[3];

    int status = fc_mat_lu_decompose_f64(3, A, 3, ipiv);
    ASSERT_EQ(status, FC_OK);

    /* Check that diagonal elements are non-zero */
    ASSERT_TRUE(fabs(A[0]) > TEST_EPSILON);
    ASSERT_TRUE(fabs(A[4]) > TEST_EPSILON);
    ASSERT_TRUE(fabs(A[8]) > TEST_EPSILON);
}

TEST(test_lu_decompose_identity) {
    double A[] = {
        1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0
    };
    int64_t ipiv[3];

    int status = fc_mat_lu_decompose_f64(3, A, 3, ipiv);
    ASSERT_EQ(status, FC_OK);

    /* Identity should remain identity */
    ASSERT_TRUE(double_equals(A[0], 1.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(A[4], 1.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(A[8], 1.0, TEST_EPSILON));
}

TEST(test_lu_decompose_invalid_args) {
    double A[9];
    int64_t ipiv[3];

    ASSERT_EQ(fc_mat_lu_decompose_f64(3, NULL, 3, ipiv), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_lu_decompose_f64(3, A, 3, NULL), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_lu_decompose_f64(0, A, 3, ipiv), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_lu_decompose_f64(-1, A, 3, ipiv), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_lu_decompose_f64(3, A, 2, ipiv), FC_ERR_INVALID_ARG);
}

TEST(test_lu_decompose_singular) {
    /* Singular matrix (row 2 = row 1) */
    double A[] = {
        1.0, 2.0, 3.0,
        1.0, 2.0, 3.0,
        4.0, 5.0, 6.0
    };
    int64_t ipiv[3];

    int status = fc_mat_lu_decompose_f64(3, A, 3, ipiv);
    ASSERT_EQ(status, FC_ERR_SINGULAR_MATRIX);
}

TEST(test_qr_decompose_basic) {
    /* Test 3x3 matrix */
    double A[] = {
        12.0, -51.0, 4.0,
        6.0, 167.0, -68.0,
        -4.0, 24.0, -41.0
    };
    double tau[3];

    int status = fc_mat_qr_decompose_f64(3, 3, A, 3, tau);
    ASSERT_EQ(status, FC_OK);

    /* R diagonal should be non-zero */
    ASSERT_TRUE(fabs(A[0]) > TEST_EPSILON);
    ASSERT_TRUE(fabs(A[4]) > TEST_EPSILON);
    ASSERT_TRUE(fabs(A[8]) > TEST_EPSILON);
}

TEST(test_qr_decompose_invalid_args) {
    double A[12];
    double tau[3];

    ASSERT_EQ(fc_mat_qr_decompose_f64(4, 3, NULL, 3, tau), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_qr_decompose_f64(4, 3, A, 3, NULL), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_qr_decompose_f64(0, 3, A, 3, tau), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_qr_decompose_f64(4, 0, A, 3, tau), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_qr_decompose_f64(2, 3, A, 3, tau), FC_ERR_DIMENSION_MISMATCH);
    ASSERT_EQ(fc_mat_qr_decompose_f64(4, 3, A, 2, tau), FC_ERR_INVALID_ARG);
}

TEST(test_qr_decompose_tall) {
    /* Test 4x3 tall matrix */
    double A[] = {
        1.0, 2.0, 3.0,
        4.0, 5.0, 6.0,
        7.0, 8.0, 9.0,
        10.0, 11.0, 12.0
    };
    double tau[3];

    int status = fc_mat_qr_decompose_f64(4, 3, A, 3, tau);
    ASSERT_EQ(status, FC_OK);
}

void test_decompose_register(void) {
    RUN_TEST(test_cholesky_decompose_basic);
    RUN_TEST(test_cholesky_decompose_identity);
    RUN_TEST(test_cholesky_decompose_invalid_args);
    RUN_TEST(test_cholesky_decompose_not_positive_def);
    RUN_TEST(test_lu_decompose_basic);
    RUN_TEST(test_lu_decompose_identity);
    RUN_TEST(test_lu_decompose_invalid_args);
    RUN_TEST(test_lu_decompose_singular);
    RUN_TEST(test_qr_decompose_basic);
    RUN_TEST(test_qr_decompose_invalid_args);
    RUN_TEST(test_qr_decompose_tall);
}
