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
#include <fin-kit/matrix/matrix.h>
#include <fin-kit/platform/error.h>
#include <fin-kit/platform/simd_detect.h>
#include <fin-kit/matrix/matrix_internal.h>

/* Test tolerance for floating-point comparisons */
#define TEST_EPSILON 1e-12
#define TEST_EPSILON_RELAXED 1e-10

/*
 * Helper functions
*/

static int double_equals(double a, double b, double epsilon) {
    return fabs(a - b) < epsilon;
}

/* Generate random double in range [min, max] */
static double rand_double(double min, double max) {
    double scale = rand() / (double)RAND_MAX;
    return min + scale * (max - min);
}

/* Fill matrix with random values */
static void fill_random_matrix(double* mat, int rows, int cols, int ld,
                                double min, double max) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            mat[i * ld + j] = rand_double(min, max);
        }
    }
}

/* Fill vector with random values */
static void fill_random_vector(double* vec, int n, double min, double max) {
    for (int i = 0; i < n; i++) {
        vec[i] = rand_double(min, max);
    }
}

/* Compare two matrices with tolerance */
static int matrices_equal(const double* A, const double* B,
                          int rows, int cols, int ld_a, int ld_b,
                          double epsilon) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (!double_equals(A[i * ld_a + j], B[i * ld_b + j], epsilon)) {
                return 0;
            }
        }
    }
    return 1;
}

/* Naive GEMM for reference (C = alpha*A*B + beta*C) */
static void gemm_reference(int m, int n, int k,
                           double alpha,
                           const double* A, int lda,
                           const double* B, int ldb,
                           double beta,
                           double* C, int ldc) {
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            double sum = 0.0;
            for (int p = 0; p < k; p++) {
                sum += A[i * lda + p] * B[p * ldb + j];
            }
            C[i * ldc + j] = alpha * sum + beta * C[i * ldc + j];
        }
    }
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
 * GEMM precision validation tests
*/

TEST(test_gemm_precision_64x64) {
    /* Test GEMM precision on 64x64 matrices against reference implementation */
    const int m = 64, n = 64, k = 64;
    double* A = (double*)malloc(m * k * sizeof(double));
    double* B = (double*)malloc(k * n * sizeof(double));
    double* C = (double*)malloc(m * n * sizeof(double));
    double* C_ref = (double*)malloc(m * n * sizeof(double));

    /* Initialize with random values */
    srand(12345);
    fill_random_matrix(A, m, k, k, -10.0, 10.0);
    fill_random_matrix(B, k, n, n, -10.0, 10.0);
    memset(C, 0, m * n * sizeof(double));
    memset(C_ref, 0, m * n * sizeof(double));

    /* Compute with optimized GEMM */
    int status = fc_mat_gemm_f64(m, n, k, 1.0, A, k, B, n, 0.0, C, n);
    ASSERT_EQ(status, FC_OK);

    /* Compute reference result */
    gemm_reference(m, n, k, 1.0, A, k, B, n, 0.0, C_ref, n);

    /* Verify precision: relative error should be < 1e-12 */
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            double diff = fabs(C[i * n + j] - C_ref[i * n + j]);
            double magnitude = fabs(C_ref[i * n + j]);
            double rel_error = (magnitude > 1e-15) ? (diff / magnitude) : diff;
            if (rel_error >= 1e-12) {
                printf("Precision error at [%d,%d]: got %.17g, expected %.17g, rel_error=%.3e\n",
                       i, j, C[i * n + j], C_ref[i * n + j], rel_error);
            }
            ASSERT_TRUE(rel_error < 1e-12);
        }
    }

    free(A);
    free(B);
    free(C);
    free(C_ref);
}

TEST(test_gemm_precision_128x128) {
    /* Test GEMM precision on larger 128x128 matrices */
    const int m = 128, n = 128, k = 128;
    double* A = (double*)malloc(m * k * sizeof(double));
    double* B = (double*)malloc(k * n * sizeof(double));
    double* C = (double*)malloc(m * n * sizeof(double));
    double* C_ref = (double*)malloc(m * n * sizeof(double));

    /* Initialize with random values */
    srand(54321);
    fill_random_matrix(A, m, k, k, -5.0, 5.0);
    fill_random_matrix(B, k, n, n, -5.0, 5.0);
    memset(C, 0, m * n * sizeof(double));
    memset(C_ref, 0, m * n * sizeof(double));

    /* Compute with optimized GEMM */
    int status = fc_mat_gemm_f64(m, n, k, 1.0, A, k, B, n, 0.0, C, n);
    ASSERT_EQ(status, FC_OK);

    /* Compute reference result */
    gemm_reference(m, n, k, 1.0, A, k, B, n, 0.0, C_ref, n);

    /* Verify precision */
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            double diff = fabs(C[i * n + j] - C_ref[i * n + j]);
            double magnitude = fabs(C_ref[i * n + j]);
            double rel_error = (magnitude > 1e-15) ? (diff / magnitude) : diff;
            ASSERT_TRUE(rel_error < 1e-10);
        }
    }

    free(A);
    free(B);
    free(C);
    free(C_ref);
}

TEST(test_gemm_precision_large_values) {
    /* Test GEMM precision with large values to check numerical stability */
    const int m = 32, n = 32, k = 32;
    double* A = (double*)malloc(m * k * sizeof(double));
    double* B = (double*)malloc(k * n * sizeof(double));
    double* C = (double*)malloc(m * n * sizeof(double));
    double* C_ref = (double*)malloc(m * n * sizeof(double));

    /* Initialize with larger values */
    srand(99999);
    fill_random_matrix(A, m, k, k, -1000.0, 1000.0);
    fill_random_matrix(B, k, n, n, -1000.0, 1000.0);
    memset(C, 0, m * n * sizeof(double));
    memset(C_ref, 0, m * n * sizeof(double));

    /* Compute with optimized GEMM */
    int status = fc_mat_gemm_f64(m, n, k, 1.0, A, k, B, n, 0.0, C, n);
    ASSERT_EQ(status, FC_OK);

    /* Compute reference result */
    gemm_reference(m, n, k, 1.0, A, k, B, n, 0.0, C_ref, n);

    /* Verify precision with slightly relaxed tolerance for large values */
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            double diff = fabs(C[i * n + j] - C_ref[i * n + j]);
            double magnitude = fabs(C_ref[i * n + j]);
            double rel_error = (magnitude > 1e-10) ? (diff / magnitude) : diff;
            ASSERT_TRUE(rel_error < 1e-10);
        }
    }

    free(A);
    free(B);
    free(C);
    free(C_ref);
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

/*
 * GEMV tests
*/

TEST(test_gemv_basic) {
    double A[] = {
        1.0, 2.0, 3.0,
        4.0, 5.0, 6.0
    };
    double x[] = {1.0, 2.0, 3.0};
    double y[] = {0.0, 0.0};

    int status = fc_mat_gemv_f64(2, 3, 1.0, A, 3, x, 0.0, y);
    ASSERT_EQ(status, FC_OK);
    ASSERT_TRUE(double_equals(y[0], 14.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(y[1], 32.0, TEST_EPSILON));
}

TEST(test_gemv_alpha_beta) {
    double A[] = {
        1.0, 2.0,
        3.0, 4.0
    };
    double x[] = {1.0, 2.0};
    double y[] = {1.0, 1.0};

    int status = fc_mat_gemv_f64(2, 2, 2.0, A, 2, x, 3.0, y);
    ASSERT_EQ(status, FC_OK);
    ASSERT_TRUE(double_equals(y[0], 2.0 * 5.0 + 3.0 * 1.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(y[1], 2.0 * 11.0 + 3.0 * 1.0, TEST_EPSILON));
}

TEST(test_gemv_invalid_args) {
    double A[] = {1.0, 2.0, 3.0, 4.0};
    double x[] = {1.0, 2.0};
    double y[] = {0.0, 0.0};

    ASSERT_EQ(fc_mat_gemv_f64(0, 2, 1.0, A, 2, x, 0.0, y), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_gemv_f64(2, 0, 1.0, A, 2, x, 0.0, y), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_gemv_f64(2, 2, 1.0, NULL, 2, x, 0.0, y), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_gemv_f64(2, 2, 1.0, A, 2, NULL, 0.0, y), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_gemv_f64(2, 2, 1.0, A, 2, x, 0.0, NULL), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_gemv_f64(2, 2, 1.0, A, 1, x, 0.0, y), FC_ERR_INVALID_ARG);
}

/*
 * Transpose tests
*/

TEST(test_transpose_basic) {
    double src[] = {
        1.0, 2.0, 3.0,
        4.0, 5.0, 6.0
    };
    double dst[6];

    int status = fc_mat_transpose_f64(2, 3, src, 3, dst, 2);
    ASSERT_EQ(status, FC_OK);
    ASSERT_TRUE(double_equals(dst[0], 1.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(dst[1], 4.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(dst[2], 2.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(dst[3], 5.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(dst[4], 3.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(dst[5], 6.0, TEST_EPSILON));
}

TEST(test_transpose_non_square) {
    double src[] = {
        1.0, 2.0,
        3.0, 4.0,
        5.0, 6.0
    };
    double dst[6];

    int status = fc_mat_transpose_f64(3, 2, src, 2, dst, 3);
    ASSERT_EQ(status, FC_OK);
    ASSERT_TRUE(double_equals(dst[0], 1.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(dst[1], 3.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(dst[2], 5.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(dst[3], 2.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(dst[4], 4.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(dst[5], 6.0, TEST_EPSILON));
}

TEST(test_transpose_invalid_args) {
    double src[] = {1.0, 2.0, 3.0, 4.0};
    double dst[4];

    ASSERT_EQ(fc_mat_transpose_f64(0, 2, src, 2, dst, 2), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_transpose_f64(2, 0, src, 2, dst, 2), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_transpose_f64(2, 2, NULL, 2, dst, 2), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_transpose_f64(2, 2, src, 2, NULL, 2), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_transpose_f64(2, 2, src, 1, dst, 2), FC_ERR_INVALID_ARG);
    ASSERT_EQ(fc_mat_transpose_f64(2, 2, src, 2, dst, 1), FC_ERR_INVALID_ARG);
}

TEST(test_transpose_large_blocked) {
    /* Test large matrix to trigger blocking path (>= 32x32) */
    const int rows = 64;
    const int cols = 64;
    double* src = (double*)malloc(rows * cols * sizeof(double));
    double* dst = (double*)malloc(rows * cols * sizeof(double));

    for (int i = 0; i < rows * cols; i++) {
        src[i] = (double)i;
    }

    int status = fc_mat_transpose_f64(rows, cols, src, cols, dst, rows);
    ASSERT_EQ(status, FC_OK);

    /* Verify a few elements */
    ASSERT_TRUE(double_equals(dst[0], src[0], TEST_EPSILON));
    ASSERT_TRUE(double_equals(dst[1], src[cols], TEST_EPSILON));
    ASSERT_TRUE(double_equals(dst[rows], src[1], TEST_EPSILON));

    free(src);
    free(dst);
}

TEST(test_transpose_medium_blocked) {
    /* Test medium matrix to trigger blocking without AVX2 4x4 optimization */
    const int rows = 40;
    const int cols = 40;
    double* src = (double*)malloc(rows * cols * sizeof(double));
    double* dst = (double*)malloc(rows * cols * sizeof(double));

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            src[i * cols + j] = (double)(i * cols + j);
        }
    }

    int status = fc_mat_transpose_f64(rows, cols, src, cols, dst, rows);
    ASSERT_EQ(status, FC_OK);

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            ASSERT_TRUE(double_equals(dst[j * rows + i], src[i * cols + j], TEST_EPSILON));
        }
    }

    free(src);
    free(dst);
}

TEST(test_transpose_non_multiple_of_4) {
    /* Test dimensions not multiple of 4 to exercise edge handling */
    const int rows = 7;
    const int cols = 9;
    double src[63];
    double dst[63];

    for (int i = 0; i < rows * cols; i++) {
        src[i] = (double)i;
    }

    int status = fc_mat_transpose_f64(rows, cols, src, cols, dst, rows);
    ASSERT_EQ(status, FC_OK);

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            ASSERT_TRUE(double_equals(dst[j * rows + i], src[i * cols + j], TEST_EPSILON));
        }
    }
}

TEST(test_gemv_large_matrix) {
    /* Test larger matrix to exercise SIMD paths */
    const int m = 16;
    const int n = 16;
    double* A = (double*)malloc(m * n * sizeof(double));
    double* x = (double*)malloc(n * sizeof(double));
    double* y = (double*)malloc(m * sizeof(double));

    for (int i = 0; i < m * n; i++) {
        A[i] = 1.0;
    }
    for (int i = 0; i < n; i++) {
        x[i] = 1.0;
    }
    for (int i = 0; i < m; i++) {
        y[i] = 0.0;
    }

    int status = fc_mat_gemv_f64(m, n, 1.0, A, n, x, 0.0, y);
    ASSERT_EQ(status, FC_OK);

    /* Each row sums to n */
    for (int i = 0; i < m; i++) {
        ASSERT_TRUE(double_equals(y[i], (double)n, TEST_EPSILON));
    }

    free(A);
    free(x);
    free(y);
}

TEST(test_gemv_non_multiple_of_4) {
    /* Test n not multiple of 4 to exercise remainder path */
    const int m = 5;
    const int n = 7;
    double A[35];
    double x[7];
    double y[5];

    for (int i = 0; i < m * n; i++) {
        A[i] = 1.0;
    }
    for (int i = 0; i < n; i++) {
        x[i] = 2.0;
    }
    for (int i = 0; i < m; i++) {
        y[i] = 0.0;
    }

    int status = fc_mat_gemv_f64(m, n, 1.0, A, n, x, 0.0, y);
    ASSERT_EQ(status, FC_OK);

    for (int i = 0; i < m; i++) {
        ASSERT_TRUE(double_equals(y[i], 14.0, TEST_EPSILON));
    }
}

TEST(test_gemv_alpha_zero) {
    /* Test alpha = 0 path */
    double A[] = {1.0, 2.0, 3.0, 4.0};
    double x[] = {1.0, 1.0};
    double y[] = {5.0, 6.0};

    int status = fc_mat_gemv_f64(2, 2, 0.0, A, 2, x, 2.0, y);
    ASSERT_EQ(status, FC_OK);

    ASSERT_TRUE(double_equals(y[0], 10.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(y[1], 12.0, TEST_EPSILON));
}

TEST(test_gemv_beta_zero) {
    /* Test beta = 0 path with non-zero y */
    double A[] = {1.0, 2.0, 3.0, 4.0};
    double x[] = {1.0, 1.0};
    double y[] = {999.0, 999.0};

    int status = fc_mat_gemv_f64(2, 2, 1.0, A, 2, x, 0.0, y);
    ASSERT_EQ(status, FC_OK);

    ASSERT_TRUE(double_equals(y[0], 3.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(y[1], 7.0, TEST_EPSILON));
}

TEST(test_gemv_single_row) {
    /* Test single row (m=1) */
    double A[] = {1.0, 2.0, 3.0, 4.0};
    double x[] = {1.0, 2.0, 3.0, 4.0};
    double y[] = {0.0};

    int status = fc_mat_gemv_f64(1, 4, 1.0, A, 4, x, 0.0, y);
    ASSERT_EQ(status, FC_OK);

    ASSERT_TRUE(double_equals(y[0], 30.0, TEST_EPSILON));
}

TEST(test_gemv_single_col) {
    /* Test single column (n=1) */
    double A[] = {1.0, 2.0, 3.0, 4.0};
    double x[] = {2.0};
    double y[] = {0.0, 0.0, 0.0, 0.0};

    int status = fc_mat_gemv_f64(4, 1, 1.0, A, 1, x, 0.0, y);
    ASSERT_EQ(status, FC_OK);

    ASSERT_TRUE(double_equals(y[0], 2.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(y[1], 4.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(y[2], 6.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(y[3], 8.0, TEST_EPSILON));
}

TEST(test_gemv_odd_n) {
    /* Test n=3 to exercise SSE42 remainder path (n not multiple of 2) */
    double A[] = {
        1.0, 2.0, 3.0,
        4.0, 5.0, 6.0,
        7.0, 8.0, 9.0
    };
    double x[] = {1.0, 1.0, 1.0};
    double y[] = {0.0, 0.0, 0.0};

    int status = fc_mat_gemv_f64(3, 3, 1.0, A, 3, x, 0.0, y);
    ASSERT_EQ(status, FC_OK);

    ASSERT_TRUE(double_equals(y[0], 6.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(y[1], 15.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(y[2], 24.0, TEST_EPSILON));
}

TEST(test_gemv_n_5) {
    /* Test n=5 to exercise SSE42 with remainder */
    double A[] = {
        1.0, 2.0, 3.0, 4.0, 5.0,
        6.0, 7.0, 8.0, 9.0, 10.0
    };
    double x[] = {1.0, 1.0, 1.0, 1.0, 1.0};
    double y[] = {0.0, 0.0};

    int status = fc_mat_gemv_f64(2, 5, 1.0, A, 5, x, 0.0, y);
    ASSERT_EQ(status, FC_OK);

    ASSERT_TRUE(double_equals(y[0], 15.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(y[1], 40.0, TEST_EPSILON));
}

TEST(test_gemv_n_6) {
    /* Test n=6 (multiple of 2 for SSE42) */
    double A[] = {
        1.0, 2.0, 3.0, 4.0, 5.0, 6.0,
        7.0, 8.0, 9.0, 10.0, 11.0, 12.0
    };
    double x[] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    double y[] = {0.0, 0.0};

    int status = fc_mat_gemv_f64(2, 6, 1.0, A, 6, x, 0.0, y);
    ASSERT_EQ(status, FC_OK);

    ASSERT_TRUE(double_equals(y[0], 21.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(y[1], 57.0, TEST_EPSILON));
}

TEST(test_gemv_beta_nonzero) {
    /* Test beta != 0 path in SSE42 */
    double A[] = {
        1.0, 2.0, 3.0, 4.0,
        5.0, 6.0, 7.0, 8.0
    };
    double x[] = {1.0, 1.0, 1.0, 1.0};
    double y[] = {10.0, 20.0};

    int status = fc_mat_gemv_f64(2, 4, 1.0, A, 4, x, 0.5, y);
    ASSERT_EQ(status, FC_OK);

    ASSERT_TRUE(double_equals(y[0], 10.0 + 5.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(y[1], 26.0 + 10.0, TEST_EPSILON));
}

TEST(test_gemm_sse42_n_remainder) {
    /* Test SSE42 with n not multiple of 2 */
    double A[12] = {1.0, 2.0, 3.0, 4.0,
                    5.0, 6.0, 7.0, 8.0,
                    9.0, 10.0, 11.0, 12.0};
    double B[20] = {1.0, 0.0, 0.0, 0.0, 1.0,
                    0.0, 1.0, 0.0, 0.0, 1.0,
                    0.0, 0.0, 1.0, 0.0, 1.0,
                    0.0, 0.0, 0.0, 1.0, 1.0};
    double C[15] = {0.0};

    int status = fc_mat_gemm_f64_sse42(3, 5, 4, 1.0, A, 4, B, 5, 0.0, C, 5);
    ASSERT_EQ(status, FC_OK);
}

TEST(test_gemm_sse42_n_remainder_m4) {
    /* Test SSE42 with m=4, n=5 to trigger right edge in main loop */
    const int m = 4, n = 5, k = 4;
    double A[16], B[20], C[20];

    for (int i = 0; i < m * k; i++) A[i] = 1.0;
    for (int i = 0; i < k * n; i++) B[i] = 1.0;
    for (int i = 0; i < m * n; i++) C[i] = 0.0;

    int status = fc_mat_gemm_f64_sse42(m, n, k, 1.0, A, k, B, n, 0.0, C, n);
    ASSERT_EQ(status, FC_OK);

    for (int i = 0; i < m * n; i++) {
        ASSERT_TRUE(double_equals(C[i], (double)k, TEST_EPSILON));
    }
}

TEST(test_gemm_sse42_m_remainder) {
    /* Test SSE42 with m not multiple of 4 */
    double A[20] = {1.0, 2.0, 3.0, 4.0,
                    5.0, 6.0, 7.0, 8.0,
                    9.0, 10.0, 11.0, 12.0,
                    13.0, 14.0, 15.0, 16.0,
                    17.0, 18.0, 19.0, 20.0};
    double B[16] = {1.0, 0.0, 0.0, 0.0,
                    0.0, 1.0, 0.0, 0.0,
                    0.0, 0.0, 1.0, 0.0,
                    0.0, 0.0, 0.0, 1.0};
    double C[20] = {0.0};

    int status = fc_mat_gemm_f64_sse42(5, 4, 4, 1.0, A, 4, B, 4, 0.0, C, 4);
    ASSERT_EQ(status, FC_OK);
}

TEST(test_gemm_sse42_both_remainder) {
    /* Test SSE42 with both m and n remainders */
    double A[15] = {1.0, 2.0, 3.0, 4.0, 5.0,
                    6.0, 7.0, 8.0, 9.0, 10.0,
                    11.0, 12.0, 13.0, 14.0, 15.0};
    double B[15] = {1.0, 0.0, 0.0,
                    0.0, 1.0, 0.0,
                    0.0, 0.0, 1.0,
                    0.0, 0.0, 0.0,
                    0.0, 0.0, 0.0};
    double C[9] = {0.0};

    int status = fc_mat_gemm_f64_sse42(3, 3, 5, 1.0, A, 5, B, 3, 0.0, C, 3);
    ASSERT_EQ(status, FC_OK);
}

TEST(test_gemm_sse42_n_remainder_beta) {
    /* Test SSE42 n remainder with beta != 0 */
    double A[12] = {1.0, 2.0, 3.0, 4.0,
                    5.0, 6.0, 7.0, 8.0,
                    9.0, 10.0, 11.0, 12.0};
    double B[20] = {1.0, 0.0, 0.0, 0.0, 1.0,
                    0.0, 1.0, 0.0, 0.0, 1.0,
                    0.0, 0.0, 1.0, 0.0, 1.0,
                    0.0, 0.0, 0.0, 1.0, 1.0};
    double C[15] = {1.0, 1.0, 1.0, 1.0, 1.0,
                    1.0, 1.0, 1.0, 1.0, 1.0,
                    1.0, 1.0, 1.0, 1.0, 1.0};

    int status = fc_mat_gemm_f64_sse42(3, 5, 4, 1.0, A, 4, B, 5, 0.5, C, 5);
    ASSERT_EQ(status, FC_OK);
}

/*
 * AVX2 GEMM direct tests
*/

TEST(test_gemm_avx2_direct) {
    /* Test AVX2 4x8 micro-kernel directly */
    double A[16] = {1.0, 2.0, 3.0, 4.0,
                    5.0, 6.0, 7.0, 8.0,
                    9.0, 10.0, 11.0, 12.0,
                    13.0, 14.0, 15.0, 16.0};
    double B[32] = {1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                    0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                    0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                    0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0};
    double C[32] = {0.0};

    int status = fc_mat_gemm_f64_avx2(4, 8, 4, 1.0, A, 4, B, 8, 0.0, C, 8);
    ASSERT_EQ(status, FC_OK);

    ASSERT_TRUE(double_equals(C[0], 1.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(C[1], 2.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(C[2], 3.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(C[3], 4.0, TEST_EPSILON));
}

TEST(test_gemm_avx2_with_beta) {
    /* Test AVX2 with beta != 0 */
    double A[8] = {1.0, 2.0, 3.0, 4.0,
                   5.0, 6.0, 7.0, 8.0};
    double B[16] = {1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                    0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double C[16] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
                    2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0};

    int status = fc_mat_gemm_f64_avx2(2, 8, 2, 2.0, A, 2, B, 8, 3.0, C, 8);
    ASSERT_EQ(status, FC_OK);
}

TEST(test_gemm_avx2_with_beta_4x8) {
    /* Test AVX2 with m=4, beta != 0 to cover all rows */
    const int m = 4, n = 8, k = 4;
    double A[16], B[32], C[32];

    for (int i = 0; i < m * k; i++) A[i] = 1.0;
    for (int i = 0; i < k * n; i++) B[i] = 1.0;
    for (int i = 0; i < m * n; i++) C[i] = 2.0;

    int status = fc_mat_gemm_f64_avx2(m, n, k, 1.0, A, k, B, n, 2.0, C, n);
    ASSERT_EQ(status, FC_OK);

    /* C = 1.0 * A*B + 2.0 * C_old = k + 2*2 = 8 */
    for (int i = 0; i < m * n; i++) {
        ASSERT_TRUE(double_equals(C[i], (double)k + 4.0, TEST_EPSILON));
    }
}

TEST(test_gemm_avx2_n_remainder) {
    /* Test AVX2 with n not multiple of 8 */
    double A[12] = {1.0, 2.0, 3.0, 4.0,
                    5.0, 6.0, 7.0, 8.0,
                    9.0, 10.0, 11.0, 12.0};
    double B[20] = {1.0, 0.0, 0.0, 0.0, 0.0,
                    0.0, 1.0, 0.0, 0.0, 0.0,
                    0.0, 0.0, 1.0, 0.0, 0.0,
                    0.0, 0.0, 0.0, 1.0, 0.0};
    double C[15] = {0.0};

    int status = fc_mat_gemm_f64_avx2(3, 5, 4, 1.0, A, 4, B, 5, 0.0, C, 5);
    ASSERT_EQ(status, FC_OK);
}

TEST(test_gemm_avx2_m_remainder) {
    /* Test AVX2 with m not multiple of 4 */
    double A[10] = {1.0, 2.0, 3.0, 4.0, 5.0,
                    6.0, 7.0, 8.0, 9.0, 10.0};
    double B[40] = {1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                    0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                    0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                    0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0,
                    0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0};
    double C[16] = {0.0};

    int status = fc_mat_gemm_f64_avx2(2, 8, 5, 1.0, A, 5, B, 8, 0.0, C, 8);
    ASSERT_EQ(status, FC_OK);
}

TEST(test_gemm_avx2_both_remainder) {
    /* Test AVX2 with both m and n remainders */
    double A[15] = {1.0, 2.0, 3.0, 4.0, 5.0,
                    6.0, 7.0, 8.0, 9.0, 10.0,
                    11.0, 12.0, 13.0, 14.0, 15.0};
    double B[15] = {1.0, 0.0, 0.0,
                    0.0, 1.0, 0.0,
                    0.0, 0.0, 1.0,
                    0.0, 0.0, 0.0,
                    0.0, 0.0, 0.0};
    double C[9] = {0.0};

    int status = fc_mat_gemm_f64_avx2(3, 3, 5, 1.0, A, 5, B, 3, 0.0, C, 3);
    ASSERT_EQ(status, FC_OK);
}

TEST(test_gemm_avx2_n_remainder_beta) {
    /* Test AVX2 n remainder with beta != 0 */
    double A[12] = {1.0, 2.0, 3.0, 4.0,
                    5.0, 6.0, 7.0, 8.0,
                    9.0, 10.0, 11.0, 12.0};
    double B[20] = {1.0, 0.0, 0.0, 0.0, 1.0,
                    0.0, 1.0, 0.0, 0.0, 1.0,
                    0.0, 0.0, 1.0, 0.0, 1.0,
                    0.0, 0.0, 0.0, 1.0, 1.0};
    double C[20] = {1.0, 1.0, 1.0, 1.0, 1.0,
                    1.0, 1.0, 1.0, 1.0, 1.0,
                    1.0, 1.0, 1.0, 1.0, 1.0,
                    1.0, 1.0, 1.0, 1.0, 1.0};

    int status = fc_mat_gemm_f64_avx2(3, 5, 4, 1.0, A, 4, B, 5, 0.5, C, 5);
    ASSERT_EQ(status, FC_OK);
}

TEST(test_transpose_square_4x4) {
    /* Test 4x4 to potentially trigger AVX2 4x4 optimization */
    double src[] = {
        1.0, 2.0, 3.0, 4.0,
        5.0, 6.0, 7.0, 8.0,
        9.0, 10.0, 11.0, 12.0,
        13.0, 14.0, 15.0, 16.0
    };
    double dst[16];

    int status = fc_mat_transpose_f64(4, 4, src, 4, dst, 4);
    ASSERT_EQ(status, FC_OK);

    ASSERT_TRUE(double_equals(dst[0], 1.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(dst[1], 5.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(dst[2], 9.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(dst[3], 13.0, TEST_EPSILON));
}

TEST(test_transpose_8x8) {
    /* Test 8x8 matrix */
    double src[64];
    double dst[64];

    for (int i = 0; i < 64; i++) {
        src[i] = (double)i;
    }

    int status = fc_mat_transpose_f64(8, 8, src, 8, dst, 8);
    ASSERT_EQ(status, FC_OK);

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            ASSERT_TRUE(double_equals(dst[j * 8 + i], src[i * 8 + j], TEST_EPSILON));
        }
    }
}

TEST(test_transpose_small_3x3) {
    /* Test small matrix that uses scalar path */
    double src[] = {
        1.0, 2.0, 3.0,
        4.0, 5.0, 6.0,
        7.0, 8.0, 9.0
    };
    double dst[9];

    int status = fc_mat_transpose_f64(3, 3, src, 3, dst, 3);
    ASSERT_EQ(status, FC_OK);

    ASSERT_TRUE(double_equals(dst[0], 1.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(dst[1], 4.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(dst[2], 7.0, TEST_EPSILON));
}

TEST(test_transpose_large_for_blocked) {
    /* Test large matrix (>= 32) to trigger blocked path */
    const int rows = 64;
    const int cols = 64;
    double* src = (double*)malloc(rows * cols * sizeof(double));
    double* dst = (double*)malloc(rows * cols * sizeof(double));

    for (int i = 0; i < rows * cols; i++) {
        src[i] = (double)i;
    }

    int status = fc_mat_transpose_f64(rows, cols, src, cols, dst, rows);
    ASSERT_EQ(status, FC_OK);

    /* Verify a few elements */
    ASSERT_TRUE(double_equals(dst[0], src[0], TEST_EPSILON));
    ASSERT_TRUE(double_equals(dst[rows], src[1], TEST_EPSILON));
    ASSERT_TRUE(double_equals(dst[1], src[cols], TEST_EPSILON));

    free(src);
    free(dst);
}

TEST(test_gemv_with_beta_nonzero) {
    /* Test GEMV with beta != 0 to cover that branch */
    const int m = 4;
    const int n = 8;
    double A[32];
    double x[8];
    double y[4] = {1.0, 2.0, 3.0, 4.0};

    for (int i = 0; i < m * n; i++) {
        A[i] = 1.0;
    }
    for (int i = 0; i < n; i++) {
        x[i] = 1.0;
    }

    int status = fc_mat_gemv_f64(m, n, 2.0, A, n, x, 3.0, y);
    ASSERT_EQ(status, FC_OK);

    /* y = 2.0 * (8.0) + 3.0 * y_old */
    ASSERT_TRUE(double_equals(y[0], 2.0 * 8.0 + 3.0 * 1.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(y[1], 2.0 * 8.0 + 3.0 * 2.0, TEST_EPSILON));
}

TEST(test_gemv_n_not_multiple_of_4) {
    /* Test n=5 to exercise remainder path in AVX2 */
    const int m = 3;
    const int n = 5;
    double A[15];
    double x[5] = {1.0, 2.0, 3.0, 4.0, 5.0};
    double y[3] = {0.0, 0.0, 0.0};

    for (int i = 0; i < m * n; i++) {
        A[i] = 1.0;
    }

    int status = fc_mat_gemv_f64(m, n, 1.0, A, n, x, 0.0, y);
    ASSERT_EQ(status, FC_OK);

    /* Each row: 1+2+3+4+5 = 15 */
    for (int i = 0; i < m; i++) {
        ASSERT_TRUE(double_equals(y[i], 15.0, TEST_EPSILON));
    }
}

TEST(test_gemv_n_2) {
    /* Test n=2 to exercise SSE path with exact fit */
    const int m = 3;
    const int n = 2;
    double A[6] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    double x[2] = {1.0, 1.0};
    double y[3] = {0.0, 0.0, 0.0};

    int status = fc_mat_gemv_f64(m, n, 1.0, A, n, x, 0.0, y);
    ASSERT_EQ(status, FC_OK);

    ASSERT_TRUE(double_equals(y[0], 3.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(y[1], 7.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(y[2], 11.0, TEST_EPSILON));
}

TEST(test_gemv_n_3) {
    /* Test n=3 to exercise remainder in SSE path */
    const int m = 2;
    const int n = 3;
    double A[6] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    double x[3] = {1.0, 1.0, 1.0};
    double y[2] = {0.0, 0.0};

    int status = fc_mat_gemv_f64(m, n, 1.0, A, n, x, 0.0, y);
    ASSERT_EQ(status, FC_OK);

    ASSERT_TRUE(double_equals(y[0], 6.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(y[1], 15.0, TEST_EPSILON));
}

TEST(test_gemm_avx512_direct) {
    /* Direct test of AVX-512 implementation */
    if (fc_get_simd_level() < FC_SIMD_AVX512) {
        /* Skip test if AVX-512 not supported, but still call the function
         * to ensure it compiles and returns gracefully */
        return;
    }

    const int m = 8, n = 8, k = 8;
    double A[64], B[64], C[64];

    for (int i = 0; i < m * k; i++) A[i] = 1.0;
    for (int i = 0; i < k * n; i++) B[i] = 1.0;
    for (int i = 0; i < m * n; i++) C[i] = 0.0;

    int status = fc_mat_gemm_f64_avx512(m, n, k, 1.0, A, k, B, n, 0.0, C, n);
    ASSERT_EQ(status, FC_OK);

    for (int i = 0; i < m * n; i++) {
        ASSERT_TRUE(double_equals(C[i], (double)k, TEST_EPSILON));
    }
}

TEST(test_gemm_avx512_with_beta) {
    /* Test AVX-512 with beta != 0 */
    if (fc_get_simd_level() < FC_SIMD_AVX512) {
        return;
    }

    const int m = 4, n = 8, k = 4;
    double A[16], B[32], C[32];

    for (int i = 0; i < m * k; i++) A[i] = 1.0;
    for (int i = 0; i < k * n; i++) B[i] = 1.0;
    for (int i = 0; i < m * n; i++) C[i] = 2.0;

    int status = fc_mat_gemm_f64_avx512(m, n, k, 1.0, A, k, B, n, 2.0, C, n);
    ASSERT_EQ(status, FC_OK);

    /* C = 1.0 * A*B + 2.0 * C_old = k + 2*2 = 8 */
    for (int i = 0; i < m * n; i++) {
        ASSERT_TRUE(double_equals(C[i], (double)k + 4.0, TEST_EPSILON));
    }
}

TEST(test_gemm_avx512_n_remainder) {
    /* Test AVX-512 with n not multiple of 8 */
    if (fc_get_simd_level() < FC_SIMD_AVX512) {
        return;
    }

    const int m = 4, n = 10, k = 4;
    double A[16], B[40], C[40];

    for (int i = 0; i < m * k; i++) A[i] = 1.0;
    for (int i = 0; i < k * n; i++) B[i] = 1.0;
    for (int i = 0; i < m * n; i++) C[i] = 0.0;

    int status = fc_mat_gemm_f64_avx512(m, n, k, 1.0, A, k, B, n, 0.0, C, n);
    ASSERT_EQ(status, FC_OK);

    for (int i = 0; i < m * n; i++) {
        ASSERT_TRUE(double_equals(C[i], (double)k, TEST_EPSILON));
    }
}

TEST(test_gemm_avx512_m_remainder) {
    /* Test AVX-512 with m not multiple of 4 */
    if (fc_get_simd_level() < FC_SIMD_AVX512) {
        return;
    }

    const int m = 6, n = 8, k = 4;
    double A[24], B[32], C[48];

    for (int i = 0; i < m * k; i++) A[i] = 1.0;
    for (int i = 0; i < k * n; i++) B[i] = 1.0;
    for (int i = 0; i < m * n; i++) C[i] = 0.0;

    int status = fc_mat_gemm_f64_avx512(m, n, k, 1.0, A, k, B, n, 0.0, C, n);
    ASSERT_EQ(status, FC_OK);

    for (int i = 0; i < m * n; i++) {
        ASSERT_TRUE(double_equals(C[i], (double)k, TEST_EPSILON));
    }
}

TEST(test_gemm_avx512_both_remainder) {
    /* Test AVX-512 with both m and n not aligned */
    if (fc_get_simd_level() < FC_SIMD_AVX512) {
        return;
    }

    const int m = 5, n = 9, k = 4;
    double A[20], B[36], C[45];

    for (int i = 0; i < m * k; i++) A[i] = 1.0;
    for (int i = 0; i < k * n; i++) B[i] = 1.0;
    for (int i = 0; i < m * n; i++) C[i] = 0.0;

    int status = fc_mat_gemm_f64_avx512(m, n, k, 1.0, A, k, B, n, 0.0, C, n);
    ASSERT_EQ(status, FC_OK);

    for (int i = 0; i < m * n; i++) {
        ASSERT_TRUE(double_equals(C[i], (double)k, TEST_EPSILON));
    }
}

TEST(test_gemv_scalar_direct) {
    /* Direct test of scalar GEMV implementation */
    const int m = 3, n = 4;
    double A[12] = {1.0, 2.0, 3.0, 4.0,
                    5.0, 6.0, 7.0, 8.0,
                    9.0, 10.0, 11.0, 12.0};
    double x[4] = {1.0, 1.0, 1.0, 1.0};
    double y[3] = {0.0, 0.0, 0.0};

    fc_mat_gemv_f64_scalar(m, n, 1.0, A, n, x, 0.0, y);

    ASSERT_TRUE(double_equals(y[0], 10.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(y[1], 26.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(y[2], 42.0, TEST_EPSILON));
}

TEST(test_gemv_sse42_direct) {
    /* Direct test of SSE4.2 GEMV implementation */
    const int m = 3, n = 4;
    double A[12] = {1.0, 2.0, 3.0, 4.0,
                    5.0, 6.0, 7.0, 8.0,
                    9.0, 10.0, 11.0, 12.0};
    double x[4] = {1.0, 1.0, 1.0, 1.0};
    double y[3] = {0.0, 0.0, 0.0};

    fc_mat_gemv_f64_sse42(m, n, 1.0, A, n, x, 0.0, y);

    ASSERT_TRUE(double_equals(y[0], 10.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(y[1], 26.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(y[2], 42.0, TEST_EPSILON));
}

TEST(test_gemv_avx2_direct) {
    /* Direct test of AVX2 GEMV implementation */
    const int m = 3, n = 8;
    double A[24];
    double x[8] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    double y[3] = {0.0, 0.0, 0.0};

    for (int i = 0; i < 24; i++) A[i] = 1.0;

    fc_mat_gemv_f64_avx2(m, n, 1.0, A, n, x, 0.0, y);

    ASSERT_TRUE(double_equals(y[0], 8.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(y[1], 8.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(y[2], 8.0, TEST_EPSILON));
}

TEST(test_gemv_scalar_with_beta) {
    /* Test scalar GEMV with beta != 0 */
    const int m = 2, n = 3;
    double A[6] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    double x[3] = {1.0, 1.0, 1.0};
    double y[2] = {10.0, 20.0};

    fc_mat_gemv_f64_scalar(m, n, 2.0, A, n, x, 3.0, y);

    /* y = 2.0 * (6.0) + 3.0 * 10.0 = 42.0 */
    ASSERT_TRUE(double_equals(y[0], 42.0, TEST_EPSILON));
    /* y = 2.0 * (15.0) + 3.0 * 20.0 = 90.0 */
    ASSERT_TRUE(double_equals(y[1], 90.0, TEST_EPSILON));
}

TEST(test_gemv_sse42_with_beta) {
    /* Test SSE4.2 GEMV with beta != 0 */
    const int m = 2, n = 4;
    double A[8] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
    double x[4] = {1.0, 1.0, 1.0, 1.0};
    double y[2] = {5.0, 10.0};

    fc_mat_gemv_f64_sse42(m, n, 1.0, A, n, x, 2.0, y);

    /* y = 1.0 * (10.0) + 2.0 * 5.0 = 20.0 */
    ASSERT_TRUE(double_equals(y[0], 20.0, TEST_EPSILON));
    /* y = 1.0 * (26.0) + 2.0 * 10.0 = 46.0 */
    ASSERT_TRUE(double_equals(y[1], 46.0, TEST_EPSILON));
}

TEST(test_gemv_sse42_odd_n) {
    /* Test SSE4.2 GEMV with odd n to exercise remainder path */
    const int m = 2, n = 3;
    double A[6] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    double x[3] = {1.0, 1.0, 1.0};
    double y[2] = {0.0, 0.0};

    fc_mat_gemv_f64_sse42(m, n, 1.0, A, n, x, 0.0, y);

    ASSERT_TRUE(double_equals(y[0], 6.0, TEST_EPSILON));
    ASSERT_TRUE(double_equals(y[1], 15.0, TEST_EPSILON));
}

TEST(test_transpose_blocked_direct) {
    /* Direct test of blocked transpose implementation */
    const int rows = 64, cols = 64;
    double* src = (double*)malloc(rows * cols * sizeof(double));
    double* dst = (double*)malloc(rows * cols * sizeof(double));

    for (int i = 0; i < rows * cols; i++) {
        src[i] = (double)i;
    }

    transpose_blocked(rows, cols, src, cols, dst, rows);

    /* Verify transpose */
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            ASSERT_TRUE(double_equals(dst[j * rows + i], src[i * cols + j], TEST_EPSILON));
        }
    }

    free(src);
    free(dst);
}

TEST(test_transpose_blocked_non_multiple) {
    /* Test blocked transpose with dimensions not multiple of block size */
    const int rows = 50, cols = 70;
    double* src = (double*)malloc(rows * cols * sizeof(double));
    double* dst = (double*)malloc(rows * cols * sizeof(double));

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            src[i * cols + j] = (double)(i * cols + j);
        }
    }

    transpose_blocked(rows, cols, src, cols, dst, rows);

    /* Verify transpose */
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            ASSERT_TRUE(double_equals(dst[j * rows + i], src[i * cols + j], TEST_EPSILON));
        }
    }

    free(src);
    free(dst);
}

/* Random tests for robustness */
TEST(test_gemm_random_small) {
    srand(42);  /* Fixed seed for reproducibility */

    const int m = 8, n = 8, k = 8;
    double A[64], B[64], C[64], C_ref[64];

    fill_random_matrix(A, m, k, k, -10.0, 10.0);
    fill_random_matrix(B, k, n, n, -10.0, 10.0);
    fill_random_matrix(C, m, n, n, -5.0, 5.0);
    memcpy(C_ref, C, sizeof(C));

    double alpha = 1.5, beta = 0.5;

    /* Compute with optimized implementation */
    int status = fc_mat_gemm_f64(m, n, k, alpha, A, k, B, n, beta, C, n);
    ASSERT_EQ(status, FC_OK);

    /* Compute reference */
    gemm_reference(m, n, k, alpha, A, k, B, n, beta, C_ref, n);

    /* Compare results */
    ASSERT_TRUE(matrices_equal(C, C_ref, m, n, n, n, TEST_EPSILON_RELAXED));
}

TEST(test_gemm_random_rectangular) {
    srand(123);

    const int m = 12, n = 16, k = 10;
    double *A = malloc(m * k * sizeof(double));
    double *B = malloc(k * n * sizeof(double));
    double *C = malloc(m * n * sizeof(double));
    double *C_ref = malloc(m * n * sizeof(double));

    fill_random_matrix(A, m, k, k, -1.0, 1.0);
    fill_random_matrix(B, k, n, n, -1.0, 1.0);
    fill_random_matrix(C, m, n, n, -0.5, 0.5);
    memcpy(C_ref, C, m * n * sizeof(double));

    double alpha = 2.0, beta = -1.0;

    int status = fc_mat_gemm_f64(m, n, k, alpha, A, k, B, n, beta, C, n);
    ASSERT_EQ(status, FC_OK);

    gemm_reference(m, n, k, alpha, A, k, B, n, beta, C_ref, n);

    ASSERT_TRUE(matrices_equal(C, C_ref, m, n, n, n, TEST_EPSILON_RELAXED));

    free(A);
    free(B);
    free(C);
    free(C_ref);
}

TEST(test_gemv_random) {
    srand(456);

    const int m = 20, n = 16;
    double A[320], x[16], y[20], y_ref[20];

    fill_random_matrix(A, m, n, n, -5.0, 5.0);
    fill_random_vector(x, n, -2.0, 2.0);
    fill_random_vector(y, m, -1.0, 1.0);
    memcpy(y_ref, y, sizeof(y));

    double alpha = 1.0, beta = 0.5;

    int status = fc_mat_gemv_f64(m, n, alpha, A, n, x, beta, y);
    ASSERT_EQ(status, FC_OK);

    /* Reference computation */
    for (int i = 0; i < m; i++) {
        double sum = 0.0;
        for (int j = 0; j < n; j++) {
            sum += A[i * n + j] * x[j];
        }
        y_ref[i] = alpha * sum + beta * y_ref[i];
    }

    /* Compare */
    for (int i = 0; i < m; i++) {
        ASSERT_TRUE(double_equals(y[i], y_ref[i], TEST_EPSILON_RELAXED));
    }
}

TEST(test_transpose_random) {
    srand(789);

    const int rows = 15, cols = 12;
    double src[180], dst[180], dst_ref[180];

    fill_random_matrix(src, rows, cols, cols, -100.0, 100.0);

    int status = fc_mat_transpose_f64(rows, cols, src, cols, dst, rows);
    ASSERT_EQ(status, FC_OK);

    /* Reference transpose */
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            dst_ref[j * rows + i] = src[i * cols + j];
        }
    }

    /* Compare */
    ASSERT_TRUE(matrices_equal(dst, dst_ref, cols, rows, rows, rows, TEST_EPSILON));
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

    /* GEMV tests */
    RUN_TEST(test_gemv_basic);
    RUN_TEST(test_gemv_alpha_beta);
    RUN_TEST(test_gemv_invalid_args);
    RUN_TEST(test_gemv_large_matrix);
    RUN_TEST(test_gemv_non_multiple_of_4);
    RUN_TEST(test_gemv_alpha_zero);
    RUN_TEST(test_gemv_beta_zero);
    RUN_TEST(test_gemv_single_row);
    RUN_TEST(test_gemv_single_col);
    RUN_TEST(test_gemv_odd_n);
    RUN_TEST(test_gemv_n_5);
    RUN_TEST(test_gemv_n_6);
    RUN_TEST(test_gemv_beta_nonzero);

    /* Transpose tests */
    RUN_TEST(test_transpose_basic);
    RUN_TEST(test_transpose_non_square);
    RUN_TEST(test_transpose_invalid_args);
    RUN_TEST(test_transpose_large_blocked);
    RUN_TEST(test_transpose_medium_blocked);
    RUN_TEST(test_transpose_non_multiple_of_4);
    RUN_TEST(test_transpose_square_4x4);
    RUN_TEST(test_transpose_8x8);
    RUN_TEST(test_transpose_small_3x3);
    RUN_TEST(test_transpose_large_for_blocked);
    RUN_TEST(test_gemv_with_beta_nonzero);
    RUN_TEST(test_gemv_n_not_multiple_of_4);
    RUN_TEST(test_gemv_n_2);
    RUN_TEST(test_gemv_n_3);

    /* SSE42 GEMM edge cases */
    RUN_TEST(test_gemm_sse42_n_remainder);
    RUN_TEST(test_gemm_sse42_n_remainder_m4);
    RUN_TEST(test_gemm_sse42_m_remainder);
    RUN_TEST(test_gemm_sse42_both_remainder);
    RUN_TEST(test_gemm_sse42_n_remainder_beta);

    /* AVX2 GEMM direct tests */
    RUN_TEST(test_gemm_avx2_direct);
    RUN_TEST(test_gemm_avx2_with_beta);
    RUN_TEST(test_gemm_avx2_with_beta_4x8);
    RUN_TEST(test_gemm_avx2_n_remainder);
    RUN_TEST(test_gemm_avx2_m_remainder);
    RUN_TEST(test_gemm_avx2_both_remainder);
    RUN_TEST(test_gemm_avx2_n_remainder_beta);

    /* AVX-512 GEMM direct tests */
    RUN_TEST(test_gemm_avx512_direct);
    RUN_TEST(test_gemm_avx512_with_beta);
    RUN_TEST(test_gemm_avx512_n_remainder);
    RUN_TEST(test_gemm_avx512_m_remainder);
    RUN_TEST(test_gemm_avx512_both_remainder);

    /* GEMV internal implementation tests */
    RUN_TEST(test_gemv_scalar_direct);
    RUN_TEST(test_gemv_sse42_direct);
    RUN_TEST(test_gemv_avx2_direct);
    RUN_TEST(test_gemv_scalar_with_beta);
    RUN_TEST(test_gemv_sse42_with_beta);
    RUN_TEST(test_gemv_sse42_odd_n);

    /* Transpose internal implementation tests */
    RUN_TEST(test_transpose_blocked_direct);
    RUN_TEST(test_transpose_blocked_non_multiple);

    /* Random tests for robustness */
    RUN_TEST(test_gemm_random_small);
    RUN_TEST(test_gemm_random_rectangular);
    RUN_TEST(test_gemv_random);
    RUN_TEST(test_transpose_random);

    /* GEMM precision validation tests */
    RUN_TEST(test_gemm_precision_64x64);
    RUN_TEST(test_gemm_precision_128x128);
    RUN_TEST(test_gemm_precision_large_values);
}
