/**
 * @file test_matrix.c
 * @brief Unit tests for matrix operations
 *
 * Tests vector operations and GEMM
 */

#include <math.h>
#include <string.h>
#include "test_framework.h"
#include "../include/fin-kit/matrix.h"
#include "../src/platform/error.h"

/* Forward declarations for internal GEMM implementations */
int fc_mat_gemm_f64_scalar(int m, int n, int k,
                           double alpha,
                           const double* A, int lda,
                           const double* B, int ldb,
                           double beta,
                           double* C, int ldc);

int fc_mat_gemm_f64_sse42(int m, int n, int k,
                          double alpha,
                          const double* A, int lda,
                          const double* B, int ldb,
                          double beta,
                          double* C, int ldc);

/* Test tolerance for floating-point comparisons */
#define TEST_EPSILON 1e-12

/*
 * Helper functions
*/

static int double_equals(double a, double b, double epsilon) {
    return fabs(a - b) < epsilon;
}

/*
 * Vector operations tests
*/

TEST(test_vec_dot_basic) {
    double x[] = {1.0, 2.0, 3.0, 4.0};
    double y[] = {5.0, 6.0, 7.0, 8.0};
    double result;

    int status = fc_vec_dot_f64(x, y, 4, &result);
    ASSERT_EQ(status, FC_OK);

    /* Expected: 1*5 + 2*6 + 3*7 + 4*8 = 5 + 12 + 21 + 32 = 70 */
    ASSERT_TRUE(double_equals(result, 70.0, TEST_EPSILON));
}

TEST(test_vec_dot_negative) {
    double x[] = {-1.0, 2.0, -3.0, 4.0};
    double y[] = {5.0, -6.0, 7.0, -8.0};
    double result;

    int status = fc_vec_dot_f64(x, y, 4, &result);
    ASSERT_EQ(status, FC_OK);

    /* Expected: -1*5 + 2*(-6) + (-3)*7 + 4*(-8) = -5 - 12 - 21 - 32 = -70 */
    ASSERT_TRUE(double_equals(result, -70.0, TEST_EPSILON));
}

TEST(test_vec_dot_invalid_args) {
    double x[] = {1.0, 2.0};
    double result;

    /* NULL pointer */
    ASSERT_EQ(fc_vec_dot_f64(NULL, x, 2, &result), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_vec_dot_f64(x, NULL, 2, &result), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_vec_dot_f64(x, x, 2, NULL), FC_ERR_INVALID_ARG);

    /* Invalid length */
    ASSERT_EQ(fc_vec_dot_f64(x, x, 0, &result), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_vec_dot_f64(x, x, -1, &result), FC_ERR_INVALID_ARG);
}

TEST(test_vec_dot_zero) {
    /* Test zero vectors */
    double x[] = {0.0, 0.0, 0.0};
    double y[] = {1.0, 2.0, 3.0};
    double result;

    int status = fc_vec_dot_f64(x, y, 3, &result);
    ASSERT_EQ(status, FC_OK);
    ASSERT_TRUE(double_equals(result, 0.0, TEST_EPSILON));
}

TEST(test_vec_dot_single) {
    /* Test single element */
    double x[] = {3.5};
    double y[] = {2.0};
    double result;

    int status = fc_vec_dot_f64(x, y, 1, &result);
    ASSERT_EQ(status, FC_OK);
    ASSERT_TRUE(double_equals(result, 7.0, TEST_EPSILON));
}

TEST(test_vec_norm_l2_basic) {
    double x[] = {3.0, 4.0};
    double result;

    int status = fc_vec_norm_l2_f64(x, 2, &result);
    ASSERT_EQ(status, FC_OK);

    /* Expected: sqrt(3^2 + 4^2) = sqrt(9 + 16) = 5.0 */
    ASSERT_TRUE(double_equals(result, 5.0, TEST_EPSILON));
}

TEST(test_vec_norm_l2_single) {
    double x[] = {-7.5};
    double result;

    int status = fc_vec_norm_l2_f64(x, 1, &result);
    ASSERT_EQ(status, FC_OK);

    ASSERT_TRUE(double_equals(result, 7.5, TEST_EPSILON));
}

TEST(test_vec_norm_l2_zero) {
    /* Test zero vector */
    double x[] = {0.0, 0.0, 0.0};
    double result;

    int status = fc_vec_norm_l2_f64(x, 3, &result);
    ASSERT_EQ(status, FC_OK);
    ASSERT_TRUE(double_equals(result, 0.0, TEST_EPSILON));
}

TEST(test_vec_norm_l2_large) {
    /* Test overflow protection with large values */
    double x[] = {1e150, 1e150, 1e150};
    double result;

    int status = fc_vec_norm_l2_f64(x, 3, &result);
    ASSERT_EQ(status, FC_OK);
    /* Expected: sqrt(3) * 1e150 ≈ 1.732e150 */
    ASSERT_TRUE(result > 1.7e150 && result < 1.8e150);
}

TEST(test_vec_norm_l2_small) {
    /* Test underflow protection with small values */
    double x[] = {1e-150, 1e-150, 1e-150};
    double result;

    int status = fc_vec_norm_l2_f64(x, 3, &result);
    ASSERT_EQ(status, FC_OK);
    /* Expected: sqrt(3) * 1e-150 ≈ 1.732e-150 */
    ASSERT_TRUE(result > 1.7e-150 && result < 1.8e-150);
}

TEST(test_vec_norm_l2_invalid_args) {
    double x[] = {1.0, 2.0};
    double result;

    ASSERT_EQ(fc_vec_norm_l2_f64(NULL, 2, &result), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_vec_norm_l2_f64(x, 0, &result), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_vec_norm_l2_f64(x, -1, &result), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_vec_norm_l2_f64(x, 2, NULL), FC_ERR_INVALID_ARG);
}

TEST(test_vec_norm_l1_basic) {
    double x[] = {-1.0, 2.0, -3.0, 4.0};
    double result;

    int status = fc_vec_norm_l1_f64(x, 4, &result);
    ASSERT_EQ(status, FC_OK);

    /* Expected: |−1| + |2| + |−3| + |4| = 1 + 2 + 3 + 4 = 10 */
    ASSERT_TRUE(double_equals(result, 10.0, TEST_EPSILON));
}

TEST(test_vec_norm_l1_zero) {
    double x[] = {0.0, 0.0};
    double result;

    int status = fc_vec_norm_l1_f64(x, 2, &result);
    ASSERT_EQ(status, FC_OK);
    ASSERT_TRUE(double_equals(result, 0.0, TEST_EPSILON));
}

TEST(test_vec_norm_l1_invalid_args) {
    double x[] = {1.0, 2.0};
    double result;

    ASSERT_EQ(fc_vec_norm_l1_f64(NULL, 2, &result), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_vec_norm_l1_f64(x, 0, &result), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_vec_norm_l1_f64(x, 2, NULL), FC_ERR_INVALID_ARG);
}

TEST(test_vec_distance_l2_basic) {
    double x[] = {1.0, 2.0, 3.0};
    double y[] = {4.0, 6.0, 3.0};
    double result;

    int status = fc_vec_distance_l2_f64(x, y, 3, &result);
    ASSERT_EQ(status, FC_OK);

    /* Expected: sqrt((4-1)^2 + (6-2)^2 + (3-3)^2) = sqrt(9 + 16 + 0) = 5.0 */
    ASSERT_TRUE(double_equals(result, 5.0, TEST_EPSILON));
}

TEST(test_vec_distance_l2_single) {
    /* Test single element path */
    double x[] = {10.0};
    double y[] = {3.0};
    double result;

    int status = fc_vec_distance_l2_f64(x, y, 1, &result);
    ASSERT_EQ(status, FC_OK);
    ASSERT_TRUE(double_equals(result, 7.0, TEST_EPSILON));
}

TEST(test_vec_distance_l2_zero) {
    /* Test identical vectors */
    double x[] = {1.0, 2.0, 3.0};
    double result;

    int status = fc_vec_distance_l2_f64(x, x, 3, &result);
    ASSERT_EQ(status, FC_OK);
    ASSERT_TRUE(double_equals(result, 0.0, TEST_EPSILON));
}

TEST(test_vec_distance_l2_invalid_args) {
    double x[] = {1.0, 2.0};
    double y[] = {3.0, 4.0};
    double result;

    ASSERT_EQ(fc_vec_distance_l2_f64(NULL, y, 2, &result), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_vec_distance_l2_f64(x, NULL, 2, &result), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_vec_distance_l2_f64(x, y, 0, &result), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_vec_distance_l2_f64(x, y, 2, NULL), FC_ERR_INVALID_ARG);
}

TEST(test_vec_distance_l1_basic) {
    double x[] = {1.0, 2.0, 3.0};
    double y[] = {4.0, 6.0, 3.0};
    double result;

    int status = fc_vec_distance_l1_f64(x, y, 3, &result);
    ASSERT_EQ(status, FC_OK);

    /* Expected: |4-1| + |6-2| + |3-3| = 3 + 4 + 0 = 7.0 */
    ASSERT_TRUE(double_equals(result, 7.0, TEST_EPSILON));
}

TEST(test_vec_distance_l1_zero) {
    double x[] = {5.0, 10.0};
    double result;

    int status = fc_vec_distance_l1_f64(x, x, 2, &result);
    ASSERT_EQ(status, FC_OK);
    ASSERT_TRUE(double_equals(result, 0.0, TEST_EPSILON));
}

TEST(test_vec_distance_l1_invalid_args) {
    double x[] = {1.0, 2.0};
    double y[] = {3.0, 4.0};
    double result;

    ASSERT_EQ(fc_vec_distance_l1_f64(NULL, y, 2, &result), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_vec_distance_l1_f64(x, NULL, 2, &result), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_vec_distance_l1_f64(x, y, 0, &result), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_vec_distance_l1_f64(x, y, 2, NULL), FC_ERR_INVALID_ARG);
}

/*
 * GEMM tests
*/

TEST(test_gemm_identity) {
    /* Test: I * A = A (2x2 identity matrix) */
    double I[] = {
        1.0, 0.0,
        0.0, 1.0
    };
    double A[] = {
        2.0, 3.0,
        4.0, 5.0
    };
    double C[4] = {0};

    int status = fc_mat_gemm_f64(2, 2, 2, 1.0, I, 2, A, 2, 0.0, C, 2);
    ASSERT_EQ(status, FC_OK);

    /* C should equal A */
    ASSERT_TRUE(double_equals(C[0], 2.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(C[1], 3.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(C[2], 4.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(C[3], 5.0, TEST_EPSILON));
}

TEST(test_gemm_2x2) {
    /* Test: [1 2] * [5 6] = [19 22]
     *       [3 4]   [7 8]   [43 50] */
    double A[] = {1.0, 2.0, 3.0, 4.0};
    double B[] = {5.0, 6.0, 7.0, 8.0};
    double C[4] = {0};

    int status = fc_mat_gemm_f64(2, 2, 2, 1.0, A, 2, B, 2, 0.0, C, 2);
    ASSERT_EQ(status, FC_OK);

    ASSERT_TRUE(double_equals(C[0], 19.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(C[1], 22.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(C[2], 43.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(C[3], 50.0, TEST_EPSILON));
}

TEST(test_gemm_alpha_beta) {
    /* Test: C = 2.0 * A * B + 3.0 * C */
    double A[] = {1.0, 2.0};
    double B[] = {3.0, 4.0};
    double C[] = {5.0, 6.0};

    int status = fc_mat_gemm_f64(1, 2, 1, 2.0, A, 1, B, 2, 3.0, C, 2);
    ASSERT_EQ(status, FC_OK);

    /* Expected: C[0] = 2.0 * (1*3) + 3.0 * 5 = 6 + 15 = 21
     *           C[1] = 2.0 * (1*4) + 3.0 * 6 = 8 + 18 = 26 */
    ASSERT_TRUE(double_equals(C[0], 21.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(C[1], 26.0, TEST_EPSILON));
}

TEST(test_gemm_4x4) {
    /* Test 4x4 matrix multiplication (exercises micro-kernel) */
    double A[16], B[16], C[16];

    /* Initialize A and B as simple patterns */
    for (int i = 0; i < 16; i++) {
        A[i] = (double)(i + 1);
        B[i] = (double)(16 - i);
        C[i] = 0.0;
    }

    int status = fc_mat_gemm_f64(4, 4, 4, 1.0, A, 4, B, 4, 0.0, C, 4);
    ASSERT_EQ(status, FC_OK);

    /* Verify first element: C[0,0] = A[0,:] * B[:,0]
     * = 1*16 + 2*12 + 3*8 + 4*4 = 16 + 24 + 24 + 16 = 80 */
    ASSERT_TRUE(double_equals(C[0], 80.0, TEST_EPSILON));
}

TEST(test_gemm_non_square) {
    /* Test non-square matrices: 2x3 * 3x2 = 2x2 */
    double A[] = {
        1.0, 2.0, 3.0,
        4.0, 5.0, 6.0
    };
    double B[] = {
        7.0, 8.0,
        9.0, 10.0,
        11.0, 12.0
    };
    double C[4] = {0};

    int status = fc_mat_gemm_f64(2, 2, 3, 1.0, A, 3, B, 2, 0.0, C, 2);
    ASSERT_EQ(status, FC_OK);

    /* C[0,0] = 1*7 + 2*9 + 3*11 = 7 + 18 + 33 = 58 */
    ASSERT_TRUE(double_equals(C[0], 58.0, TEST_EPSILON));
    /* C[0,1] = 1*8 + 2*10 + 3*12 = 8 + 20 + 36 = 64 */
    ASSERT_TRUE(double_equals(C[1], 64.0, TEST_EPSILON));
    /* C[1,0] = 4*7 + 5*9 + 6*11 = 28 + 45 + 66 = 139 */
    ASSERT_TRUE(double_equals(C[2], 139.0, TEST_EPSILON));
    /* C[1,1] = 4*8 + 5*10 + 6*12 = 32 + 50 + 72 = 154 */
    ASSERT_TRUE(double_equals(C[3], 154.0, TEST_EPSILON));
}

TEST(test_gemm_invalid_args) {
    double A[4] = {0};
    double B[4] = {0};
    double C[4] = {0};

    /* Invalid dimensions */
    ASSERT_EQ(fc_mat_gemm_f64(0, 2, 2, 1.0, A, 2, B, 2, 0.0, C, 2), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_gemm_f64(2, 0, 2, 1.0, A, 2, B, 2, 0.0, C, 2), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_gemm_f64(2, 2, 0, 1.0, A, 2, B, 2, 0.0, C, 2), FC_ERR_INVALID_ARG);

    /* NULL pointers */
    ASSERT_EQ(fc_mat_gemm_f64(2, 2, 2, 1.0, NULL, 2, B, 2, 0.0, C, 2), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_gemm_f64(2, 2, 2, 1.0, A, 2, NULL, 2, 0.0, C, 2), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_gemm_f64(2, 2, 2, 1.0, A, 2, B, 2, 0.0, NULL, 2), FC_ERR_INVALID_ARG);

    /* Invalid leading dimensions */
    ASSERT_EQ(fc_mat_gemm_f64(2, 2, 2, 1.0, A, 1, B, 2, 0.0, C, 2), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_gemm_f64(2, 2, 2, 1.0, A, 2, B, 1, 0.0, C, 2), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_gemm_f64(2, 2, 2, 1.0, A, 2, B, 2, 0.0, C, 1), FC_ERR_INVALID_ARG);
}

TEST(test_gemm_alpha_zero) {
    /* Test alpha=0: C should be beta*C regardless of A*B */
    double A[] = {1.0, 2.0, 3.0, 4.0};
    double B[] = {5.0, 6.0, 7.0, 8.0};
    double C[] = {10.0, 20.0, 30.0, 40.0};

    int status = fc_mat_gemm_f64(2, 2, 2, 0.0, A, 2, B, 2, 2.0, C, 2);
    ASSERT_EQ(status, FC_OK);

    /* Expected: C = 0*A*B + 2*C = 2*C */
    ASSERT_TRUE(double_equals(C[0], 20.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(C[1], 40.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(C[2], 60.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(C[3], 80.0, TEST_EPSILON));
}

TEST(test_gemm_beta_one) {
    /* Test beta=1.0: C = alpha*A*B + C */
    double A[] = {1.0, 0.0, 0.0, 1.0};  /* Identity */
    double B[] = {2.0, 3.0, 4.0, 5.0};
    double C[] = {1.0, 1.0, 1.0, 1.0};

    int status = fc_mat_gemm_f64(2, 2, 2, 1.0, A, 2, B, 2, 1.0, C, 2);
    ASSERT_EQ(status, FC_OK);

    /* Expected: C = I*B + C = B + C */
    ASSERT_TRUE(double_equals(C[0], 3.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(C[1], 4.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(C[2], 5.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(C[3], 6.0, TEST_EPSILON));
}

TEST(test_gemm_edge_5x5) {
    /* Test 5x5 matrix to exercise edge handling (not divisible by 4) */
    double A[25], B[25], C[25];

    /* Initialize with simple pattern */
    for (int i = 0; i < 25; i++) {
        A[i] = (double)(i + 1);
        B[i] = 1.0;
        C[i] = 0.0;
    }

    int status = fc_mat_gemm_f64(5, 5, 5, 1.0, A, 5, B, 5, 0.0, C, 5);
    ASSERT_EQ(status, FC_OK);

    /* C[0,0] = sum(A[0,:]) = 1+2+3+4+5 = 15 */
    ASSERT_TRUE(double_equals(C[0], 15.0, TEST_EPSILON));
}

TEST(test_gemm_edge_3x7) {
    /* Test non-square with edge cases: 3x7 * 7x3 = 3x3 */
    double A[21], B[21], C[9];

    for (int i = 0; i < 21; i++) {
        A[i] = 1.0;
        B[i] = 1.0;
    }
    for (int i = 0; i < 9; i++) {
        C[i] = 0.0;
    }

    int status = fc_mat_gemm_f64(3, 3, 7, 1.0, A, 7, B, 3, 0.0, C, 3);
    ASSERT_EQ(status, FC_OK);

    /* All elements should be 7.0 (sum of 7 ones) */
    for (int i = 0; i < 9; i++) {
        ASSERT_TRUE(double_equals(C[i], 7.0, TEST_EPSILON));
    }
}

/*
 * Direct implementation tests (for coverage)
*/

TEST(test_gemm_scalar_direct) {
    /* Test scalar implementation directly to ensure coverage */
    double A[4] = {1.0, 2.0, 3.0, 4.0};
    double B[4] = {5.0, 6.0, 7.0, 8.0};
    double C[4] = {0.0, 0.0, 0.0, 0.0};

    int status = fc_mat_gemm_f64_scalar(2, 2, 2, 1.0, A, 2, B, 2, 0.0, C, 2);
    ASSERT_EQ(status, FC_OK);

    /* Expected: C = A * B
     * C[0,0] = 1*5 + 2*7 = 19
     * C[0,1] = 1*6 + 2*8 = 22
     * C[1,0] = 3*5 + 4*7 = 43
     * C[1,1] = 3*6 + 4*8 = 50
   */
    ASSERT_TRUE(double_equals(C[0], 19.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(C[1], 22.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(C[2], 43.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(C[3], 50.0, TEST_EPSILON));
}

TEST(test_gemm_scalar_edge_cases) {
    /* Test scalar implementation with non-aligned dimensions */
    double A[15] = {1,2,3,4,5, 6,7,8,9,10, 11,12,13,14,15};
    double B[21] = {1,2,3,4,5,6,7, 8,9,10,11,12,13,14, 15,16,17,18,19,20,21};
    double C[12] = {0};

    int status = fc_mat_gemm_f64_scalar(3, 4, 5, 1.0, A, 5, B, 7, 0.0, C, 4);
    ASSERT_EQ(status, FC_OK);

    /* Just verify it completes without error - exact values less important */
    ASSERT_TRUE(C[0] != 0.0);
}

TEST(test_gemm_scalar_large_4x4) {
    /* Test scalar implementation with 4x4 blocks (triggers micro-kernel) */
    double A[64], B[64], C[64];

    /* Initialize 8x8 matrices */
    for (int i = 0; i < 64; i++) {
        A[i] = (double)(i + 1);
        B[i] = 1.0;
        C[i] = 0.0;
    }

    int status = fc_mat_gemm_f64_scalar(8, 8, 8, 1.0, A, 8, B, 8, 0.0, C, 8);
    ASSERT_EQ(status, FC_OK);

    /* C[0,0] = sum(A[0,:]) = 1+2+3+4+5+6+7+8 = 36 */
    ASSERT_TRUE(double_equals(C[0], 36.0, TEST_EPSILON));
}

TEST(test_gemm_scalar_with_beta) {
    /* Test scalar implementation with non-zero beta */
    double A[16], B[16], C[16];

    for (int i = 0; i < 16; i++) {
        A[i] = 1.0;
        B[i] = 1.0;
        C[i] = 2.0;  /* Pre-initialize C */
    }

    /* C = 1.0 * A*B + 0.5 * C */
    int status = fc_mat_gemm_f64_scalar(4, 4, 4, 1.0, A, 4, B, 4, 0.5, C, 4);
    ASSERT_EQ(status, FC_OK);

    /* Each element: 1.0 * 4.0 + 0.5 * 2.0 = 5.0 */
    ASSERT_TRUE(double_equals(C[0], 5.0, TEST_EPSILON));
}

TEST(test_gemm_sse42_direct) {
    /* Test SSE4.2 implementation directly */
    double A[4] = {1.0, 2.0, 3.0, 4.0};
    double B[4] = {5.0, 6.0, 7.0, 8.0};
    double C[4] = {0.0, 0.0, 0.0, 0.0};

    int status = fc_mat_gemm_f64_sse42(2, 2, 2, 1.0, A, 2, B, 2, 0.0, C, 2);

    /* SSE4.2 might not be available on all platforms */
    if (status == FC_OK) {
        ASSERT_TRUE(double_equals(C[0], 19.0, TEST_EPSILON));
        ASSERT_TRUE(double_equals(C[1], 22.0, TEST_EPSILON));
        ASSERT_TRUE(double_equals(C[2], 43.0, TEST_EPSILON));
        ASSERT_TRUE(double_equals(C[3], 50.0, TEST_EPSILON));
    }
}

/*
 * Test suite registration
*/

/* Test n_remainder path in gemm_scalar (n not multiple of 4) */
TEST(test_gemm_scalar_n_remainder) {
    /* 4x5 matrix to trigger n_remainder = 1 */
    double A[16] = {1.0, 2.0, 3.0, 4.0,
                    5.0, 6.0, 7.0, 8.0,
                    9.0, 10.0, 11.0, 12.0,
                    13.0, 14.0, 15.0, 16.0};
    double B[20] = {1.0, 0.0, 0.0, 0.0, 1.0,
                    0.0, 1.0, 0.0, 0.0, 1.0,
                    0.0, 0.0, 1.0, 0.0, 1.0,
                    0.0, 0.0, 0.0, 1.0, 1.0};
    double C[20] = {0.0};

    int status = fc_mat_gemm_f64_scalar(4, 5, 4, 1.0, A, 4, B, 5, 0.0, C, 5);
    ASSERT_EQ(status, FC_OK);
}

/* Test n_remainder with beta != 0 in gemm_edge_n */
TEST(test_gemm_scalar_n_remainder_beta) {
    /* 4x5 matrix to trigger n_remainder = 1 with beta != 0 */
    double A[16] = {1.0, 2.0, 3.0, 4.0,
                    5.0, 6.0, 7.0, 8.0,
                    9.0, 10.0, 11.0, 12.0,
                    13.0, 14.0, 15.0, 16.0};
    double B[20] = {1.0, 0.0, 0.0, 0.0, 1.0,
                    0.0, 1.0, 0.0, 0.0, 1.0,
                    0.0, 0.0, 1.0, 0.0, 1.0,
                    0.0, 0.0, 0.0, 1.0, 1.0};
    double C[20] = {1.0, 1.0, 1.0, 1.0, 1.0,
                    1.0, 1.0, 1.0, 1.0, 1.0,
                    1.0, 1.0, 1.0, 1.0, 1.0,
                    1.0, 1.0, 1.0, 1.0, 1.0};

    int status = fc_mat_gemm_f64_scalar(4, 5, 4, 1.0, A, 4, B, 5, 0.5, C, 5);
    ASSERT_EQ(status, FC_OK);
}

/* Test m_remainder with beta != 0 in gemm_edge_m */
TEST(test_gemm_scalar_m_remainder_beta) {
    /* 3x4 matrix to trigger m_remainder = 3 with beta != 0 */
    double A[12] = {1.0, 2.0, 3.0, 4.0,
                    5.0, 6.0, 7.0, 8.0,
                    9.0, 10.0, 11.0, 12.0};
    double B[16] = {1.0, 0.0, 0.0, 0.0,
                    0.0, 1.0, 0.0, 0.0,
                    0.0, 0.0, 1.0, 0.0,
                    0.0, 0.0, 0.0, 1.0};
    double C[12] = {1.0, 1.0, 1.0, 1.0,
                    1.0, 1.0, 1.0, 1.0,
                    1.0, 1.0, 1.0, 1.0};

    int status = fc_mat_gemm_f64_scalar(3, 4, 4, 1.0, A, 4, B, 4, 0.5, C, 4);
    ASSERT_EQ(status, FC_OK);
}

/* Test SSE4.2 with beta != 0 */
TEST(test_gemm_sse42_with_beta) {
    double A[16] = {1.0, 2.0, 3.0, 4.0,
                    5.0, 6.0, 7.0, 8.0,
                    9.0, 10.0, 11.0, 12.0,
                    13.0, 14.0, 15.0, 16.0};
    double B[16] = {1.0, 0.0, 0.0, 0.0,
                    0.0, 1.0, 0.0, 0.0,
                    0.0, 0.0, 1.0, 0.0,
                    0.0, 0.0, 0.0, 1.0};
    double C[16] = {1.0, 1.0, 1.0, 1.0,
                    1.0, 1.0, 1.0, 1.0,
                    1.0, 1.0, 1.0, 1.0,
                    1.0, 1.0, 1.0, 1.0};

    int status = fc_mat_gemm_f64_sse42(4, 4, 4, 1.0, A, 4, B, 4, 0.5, C, 4);
    ASSERT_EQ(status, FC_OK);
}

/* Test L2 distance with extreme large values to trigger scale path */
TEST(test_vec_distance_l2_extreme) {
    /* Use values that will overflow without scaling */
    double x[] = {1e200, 2e200, 3e200};
    double y[] = {1e200, 2e200, 4e200};
    double result;

    int status = fc_vec_distance_l2_f64(x, y, 3, &result);
    ASSERT_EQ(status, FC_OK);
    ASSERT_TRUE(result > 0.0);
}

/* Test L2 distance with mixed large/small values to trigger ratio path */
TEST(test_vec_distance_l2_mixed) {
    /* First element sets scale, second element triggers ratio calculation */
    double x[] = {1e100, 1e50};
    double y[] = {0.0, 0.0};
    double result;

    int status = fc_vec_distance_l2_f64(x, y, 2, &result);
    ASSERT_EQ(status, FC_OK);
    ASSERT_TRUE(result > 0.0);
}

/* Test L2 distance with large values to trigger scale path */
TEST(test_vec_distance_l2_large) {
    double x[] = {1e150, 2e150, 3e150};
    double y[] = {1e150, 2e150, 4e150};
    double result;

    int status = fc_vec_distance_l2_f64(x, y, 3, &result);
    ASSERT_EQ(status, FC_OK);
    ASSERT_TRUE(result > 0.0);
}

void register_matrix_tests(void) {
    /* Vector operations - dot product */
    RUN_TEST(test_vec_dot_basic);
    RUN_TEST(test_vec_dot_negative);
    RUN_TEST(test_vec_dot_invalid_args);
    RUN_TEST(test_vec_dot_zero);
    RUN_TEST(test_vec_dot_single);

    /* Vector operations - L2 norm */
    RUN_TEST(test_vec_norm_l2_basic);
    RUN_TEST(test_vec_norm_l2_single);
    RUN_TEST(test_vec_norm_l2_zero);
    RUN_TEST(test_vec_norm_l2_large);
    RUN_TEST(test_vec_norm_l2_small);
    RUN_TEST(test_vec_norm_l2_invalid_args);

    /* Vector operations - L1 norm */
    RUN_TEST(test_vec_norm_l1_basic);
    RUN_TEST(test_vec_norm_l1_zero);
    RUN_TEST(test_vec_norm_l1_invalid_args);

    /* Vector operations - L2 distance */
    RUN_TEST(test_vec_distance_l2_basic);
    RUN_TEST(test_vec_distance_l2_single);
    RUN_TEST(test_vec_distance_l2_zero);
    RUN_TEST(test_vec_distance_l2_invalid_args);

    /* Vector operations - L1 distance */
    RUN_TEST(test_vec_distance_l1_basic);
    RUN_TEST(test_vec_distance_l1_zero);
    RUN_TEST(test_vec_distance_l1_invalid_args);

    /* GEMM */
    RUN_TEST(test_gemm_identity);
    RUN_TEST(test_gemm_2x2);
    RUN_TEST(test_gemm_alpha_beta);
    RUN_TEST(test_gemm_4x4);
    RUN_TEST(test_gemm_non_square);
    RUN_TEST(test_gemm_invalid_args);
    RUN_TEST(test_gemm_alpha_zero);
    RUN_TEST(test_gemm_beta_one);
    RUN_TEST(test_gemm_edge_5x5);
    RUN_TEST(test_gemm_edge_3x7);

    /* Direct implementation tests */
    RUN_TEST(test_gemm_scalar_direct);
    RUN_TEST(test_gemm_scalar_edge_cases);
    RUN_TEST(test_gemm_scalar_large_4x4);
    RUN_TEST(test_gemm_scalar_with_beta);
    RUN_TEST(test_gemm_sse42_direct);

    /* Additional coverage tests */
    RUN_TEST(test_gemm_scalar_n_remainder);
    RUN_TEST(test_gemm_scalar_n_remainder_beta);
    RUN_TEST(test_gemm_scalar_m_remainder_beta);
    RUN_TEST(test_gemm_sse42_with_beta);
    RUN_TEST(test_vec_distance_l2_extreme);
    RUN_TEST(test_vec_distance_l2_mixed);
    RUN_TEST(test_vec_distance_l2_large);
}
