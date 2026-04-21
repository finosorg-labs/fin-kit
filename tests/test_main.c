/**
 * @file test_main.c
 * @brief Test runner entry point
 */

#include "test_framework.h"
#include <fin-kit/fin-kit.h>
#include <stdio.h>
#include <stdlib.h>

/* External test registration functions */
extern void register_matrix_tests(void);
extern void register_precision_tests(void);
extern void register_fix_codec_tests(void);

int main(int argc, char** argv) {
    /* Initialize fin-kit library */
    fc_init();

    /* Initialize test framework with command line arguments */
    fc_test_init_with_args(argc, argv);

    /* Register all test suites */
    register_matrix_tests();
    register_precision_tests();
    register_fix_codec_tests();

    int result = fc_test_run_all();

    /* Generate coverage report if requested */
    fc_test_generate_coverage_report();

    fc_test_cleanup();

    printf("\nfin-kit self-test: %s\n", result == 0 ? "PASS" : "FAIL");
    return result;
}
