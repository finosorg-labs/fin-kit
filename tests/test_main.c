/**
 * @file test_main.c
 * @brief Test runner entry point
 */

#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    fc_test_init();

    int result = fc_test_run_all();

    fc_test_cleanup();

    printf("\nfin-kit self-test: %s\n", result == 0 ? "PASS" : "FAIL");
    return result;
}
