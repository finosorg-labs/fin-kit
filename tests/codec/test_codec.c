/**
 * @file test_codec.c
 * @brief Codec module test entry point
 *
 * This file serves as the main test registration point for the codec module.
 * Individual test modules are in separate files:
 * - test_fix_codec.c: FIX protocol codec tests
 */

#include "test_framework.h"

/* External test registration functions from sub-modules */
extern void register_fix_codec_tests(void);

/* Entry point for codec tests */
void register_codec_tests(void) {
    /* Register all sub-module tests */
    register_fix_codec_tests();
}
