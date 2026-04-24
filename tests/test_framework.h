/**
 * @file test_framework.h
 * @brief Lightweight C unit testing framework for fin-kit
 *
 * Provides a simple but effective testing framework with:
 * - Test suite organization
 * - Assertion macros
 * - Test result reporting
 * - Memory leak detection hooks
 */

#ifndef FC_TEST_FRAMEWORK_H
#define FC_TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <platform.h>

/*
 * Test framework configuration
*/

#define FC_TEST_VERSION "1.0.0"

/**
 * @brief Maximum number of test suites
 */
#define FC_TEST_MAX_SUITES 64

/**
 * @brief Maximum number of tests per suite
 */
#define FC_TEST_MAX_TESTS_PER_SUITE 256

/**
 * @brief Maximum assertion message length
 */
#define FC_TEST_MAX_MSG_LEN 512

/*
 * Test result types
*/

/**
 * @brief Test result enumeration
 */
typedef enum {
    FC_TEST_PASSED = 0,
    FC_TEST_FAILED = 1,
    FC_TEST_SKIPPED = 2,
} fc_test_result_t;

/*
 * Test function pointer type
*/

typedef void (*fc_test_fn)(void);

/*
 * Test statistics
*/

/**
 * @brief Statistics for a test run
 */
typedef struct {
    int total_tests;
    int passed;
    int failed;
    int skipped;
    double elapsed_time_ms;
} fc_test_stats_t;

/**
 * @brief Initialize test statistics
 */
void fc_test_stats_init(fc_test_stats_t* stats);

/**
 * @brief Print test statistics to stdout
 */
void fc_test_stats_print(const fc_test_stats_t* stats);

/*
 * Assertion macros
*/

/**
 * @brief Basic assertion that condition is true
 */
#define FC_TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            fc_test_assert_fail(#cond, __FILE__, __LINE__, NULL); \
            return; \
        } \
    } while (0)

/**
 * @brief Assertion with custom failure message
 *
 * Note: Uses GNU extension ##__VA_ARGS__ for optional variadic arguments.
 * This is widely supported by GCC, Clang, and MSVC.
 */
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif
#define FC_TEST_ASSERT_MSG(cond, msg, ...) \
    do { \
        if (!(cond)) { \
            char _buf[FC_TEST_MAX_MSG_LEN]; \
            snprintf(_buf, sizeof(_buf), msg, ##__VA_ARGS__); \
            fc_test_assert_fail(#cond, __FILE__, __LINE__, _buf); \
            return; \
        } \
    } while (0)
#ifdef __clang__
#pragma clang diagnostic pop
#endif

/**
 * @brief Assert that two values are equal
 */
#define FC_TEST_ASSERT_EQ(actual, expected) \
    do { \
        if ((actual) != (expected)) { \
            fc_test_assert_fail_eq(#actual " == " #expected, __FILE__, __LINE__, \
                (intmax_t)(actual), (intmax_t)(expected)); \
            return; \
        } \
    } while (0)

/**
 * @brief Assert that two values are not equal
 */
#define FC_TEST_ASSERT_NE(actual, expected) \
    do { \
        if ((actual) == (expected)) { \
            fc_test_assert_fail_ne(#actual " != " #expected, __FILE__, __LINE__); \
            return; \
        } \
    } while (0)

/**
 * @brief Assert that value is zero
 */
#define FC_TEST_ASSERT_ZERO(val) FC_TEST_ASSERT_EQ(val, 0)

/**
 * @brief Assert that value is non-zero
 */
#define FC_TEST_ASSERT_NONZERO(val) FC_TEST_ASSERT_NE(val, 0)

/**
 * @brief Assert that two doubles are approximately equal
 * @param tolerance Relative tolerance for comparison
 */
#define FC_TEST_ASSERT_DOUBLE_EQ(actual, expected, tolerance) \
    do { \
        double _actual = (actual); \
        double _expected = (expected); \
        double _diff = (_actual - _expected); \
        double _abs_expected = (_expected >= 0 ? _expected : -_expected); \
        double _rel_diff = _abs_expected > 0 ? _diff / _abs_expected : _diff; \
        if (_rel_diff < -(tolerance) || _rel_diff > (tolerance)) { \
            fc_test_assert_fail_double(#actual " ~= " #expected, __FILE__, __LINE__, \
                _actual, _expected); \
            return; \
        } \
    } while (0)

/**
 * @brief Assert that two doubles are exactly equal (bit-for-bit)
 */
#define FC_TEST_ASSERT_DOUBLE_EQ_EXACT(actual, expected) \
    do { \
        union { double d; uint64_t u; } _a, _e; \
        _a.d = (actual); _e.d = (expected); \
        if (_a.u != _e.u) { \
            fc_test_assert_fail_double(#actual " == " #expected " (exact)", __FILE__, __LINE__, \
                (actual), (expected)); \
            return; \
        } \
    } while (0)

/**
 * @brief Assert that pointer is NULL
 */
#define FC_TEST_ASSERT_NULL(ptr) FC_TEST_ASSERT_EQ((intptr_t)(ptr), 0)

/**
 * @brief Assert that pointer is not NULL
 */
#define FC_TEST_ASSERT_NOT_NULL(ptr) FC_TEST_ASSERT_NE((intptr_t)(ptr), 0)

/**
 * @brief Assert that string equals expected value
 */
#define FC_TEST_ASSERT_STR_EQ(actual, expected) \
    do { \
        if (strcmp((actual), (expected)) != 0) { \
            fc_test_assert_fail_str(#actual " == " #expected, __FILE__, __LINE__, \
                (actual), (expected)); \
            return; \
        } \
    } while (0)

/**
 * @brief Assert that memory blocks are equal
 */
#define FC_TEST_ASSERT_MEM_EQ(actual, expected, size) \
    do { \
        if (memcmp((actual), (expected), (size)) != 0) { \
            fc_test_assert_fail_mem(#actual " == " #expected, __FILE__, __LINE__, \
                (actual), (expected), (size)); \
            return; \
        } \
    } while (0)

/**
 * @brief Mark test as skipped with message
 */
#define FC_TEST_SKIP(msg, ...) \
    do { \
        char _buf[FC_TEST_MAX_MSG_LEN]; \
        snprintf(_buf, sizeof(_buf), msg, ##__VA_ARGS__); \
        fc_test_skip(_buf); \
        return; \
    } while (0)

/**
 * @brief Unconditional test failure
 */
#define FC_TEST_FAIL(msg, ...) \
    do { \
        char _buf[FC_TEST_MAX_MSG_LEN]; \
        snprintf(_buf, sizeof(_buf), msg, ##__VA_ARGS__); \
        fc_test_assert_fail("FC_TEST_FAIL", __FILE__, __LINE__, _buf); \
        return; \
    } while (0)

/*
 * Internal assertion failure functions
*/

FC_BEGIN_DECLS

/**
 * @brief Report assertion failure
 */
FC_API void fc_test_assert_fail(
    const char* condition,
    const char* file,
    int line,
    const char* message
);

/**
 * @brief Report equality assertion failure
 */
FC_API void fc_test_assert_fail_eq(
    const char* condition,
    const char* file,
    int line,
    intmax_t actual,
    intmax_t expected
);

/**
 * @brief Report inequality assertion failure
 */
FC_API void fc_test_assert_fail_ne(
    const char* condition,
    const char* file,
    int line
);

/**
 * @brief Report double comparison failure
 */
FC_API void fc_test_assert_fail_double(
    const char* condition,
    const char* file,
    int line,
    double actual,
    double expected
);

/**
 * @brief Report string comparison failure
 */
FC_API void fc_test_assert_fail_str(
    const char* condition,
    const char* file,
    int line,
    const char* actual,
    const char* expected
);

/**
 * @brief Report memory comparison failure
 */
FC_API void fc_test_assert_fail_mem(
    const char* condition,
    const char* file,
    int line,
    const void* actual,
    const void* expected,
    size_t size
);

/**
 * @brief Mark current test as skipped
 */
FC_API void fc_test_skip(const char* message);

FC_END_DECLS

/*
 * Test suite management
*/

/**
 * @brief Test suite structure
 */
typedef struct {
    const char* name;
    const char* description;
    fc_test_fn* tests;
    int num_tests;
} fc_test_suite_t;

/**
 * @brief Register a test suite
 *
 * @param suite Pointer to test suite definition
 */
void fc_test_register_suite(const fc_test_suite_t* suite);

/**
 * @brief Run all registered test suites
 *
 * @return 0 if all tests passed, non-zero otherwise
 */
int fc_test_run_all(void);

/**
 * @brief Run a specific test suite by name
 *
 * @param name Suite name
 * @return 0 if suite passed, non-zero otherwise
 */
int fc_test_run_suite(const char* name);

/**
 * @brief Get global test statistics
 */
fc_test_stats_t* fc_test_get_stats(void);

/**
 * @brief Set test output verbosity
 *
 * @param verbose 1 for verbose output, 0 for summary only
 */
void fc_test_set_verbose(int verbose);

/**
 * @brief Enable coverage reporting
 *
 * @param enable 1 to enable, 0 to disable
 */
void fc_test_set_coverage(int enable);

/**
 * @brief Generate coverage report
 *
 * Generates coverage report using gcov if enabled.
 * Should be called after all tests complete.
 *
 * @return 0 on success, non-zero on error
 */
int fc_test_generate_coverage_report(void);

/**
 * @brief Set test filter pattern
 *
 * Only tests matching the pattern will be run.
 * Supports simple wildcard: "suite_name/test_name"
 *
 * @param pattern Filter pattern, NULL to run all tests
 */
void fc_test_set_filter(const char* pattern);

/**
 * @brief Initialize test framework
 *
 * @param argc Command line argument count
 * @param argv Command line arguments
 */
void fc_test_init_with_args(int argc, char** argv);

/**
 * @brief Initialize test framework
 */
void fc_test_init(void);

/**
 * @brief Cleanup test framework
 */
void fc_test_cleanup(void);

/*
 * Test suite registration macros
*/

/**
 * @brief Define a test function
 */
#define FC_TEST(suite_name, test_name) \
    static void suite_name##_##test_name##_impl(void); \
    static void suite_name##_##test_name##_wrapper(void) { \
        fc_test_start(#suite_name "/" #test_name); \
        suite_name##_##test_name##_impl(); \
    } \
    static void suite_name##_##test_name##_impl(void)

/**
 * @brief Declare a test suite
 */
#define FC_TEST_SUITE(suite_name) \
    static fc_test_fn suite_name##_tests[] =

/**
 * @brief Register test suite and run it
 */
#define FC_TEST_REGISTER_AND_RUN(suite) \
    do { \
        fc_test_register_suite(suite); \
        fc_test_run_suite(#suite); \
    } while (0)

/*
 * Internal test runner state
*/

FC_BEGIN_DECLS

/**
 * @brief Start a test (called by FC_TEST macro)
 */
FC_API void fc_test_start(const char* test_name);

/**
 * @brief End a test
 */
FC_API void fc_test_end(void);

/**
 * @brief Get current test result
 */
FC_API fc_test_result_t fc_test_get_result(void);

/**
 * @brief Set current test result
 */
FC_API void fc_test_set_result(fc_test_result_t result);

/**
 * @brief Get elapsed time for current test in milliseconds
 */
FC_API double fc_test_get_elapsed_ms(void);

FC_END_DECLS

/*
 * Memory tracking utilities
*/

/**
 * @brief Enable memory leak detection
 *
 * Records allocation sites for leak detection.
 */
void fc_test_enable_leak_detection(void);

/**
 * @brief Check for memory leaks
 *
 * @return Number of leaks found
 */
int fc_test_check_leaks(void);

/**
 * @brief Print memory leak report
 */
void fc_test_print_leak_report(void);

/*
 * Convenience macros for simpler test syntax
*/

/**
 * @brief Define a simple test function
 */
#define TEST(name) \
    static void name##_impl(void); \
    static void name(void) { \
        fc_test_start(#name); \
        name##_impl(); \
        fc_test_end(); \
    } \
    static void name##_impl(void)

/**
 * @brief Run a test function
 */
#define RUN_TEST(test_func) \
    do { \
        test_func(); \
    } while (0)

/**
 * @brief Assert equality (uses FC_TEST_ASSERT_EQ)
 */
#define ASSERT_EQ(actual, expected) FC_TEST_ASSERT_EQ((actual), (expected))

/**
 * @brief Assert inequality (uses FC_TEST_ASSERT_NE)
 */
#define ASSERT_NE(actual, expected) FC_TEST_ASSERT_NE((actual), (expected))

/**
 * @brief Assert true (uses FC_TEST_ASSERT)
 */
#define ASSERT_TRUE(cond) FC_TEST_ASSERT(cond)

/**
 * @brief Assert false
 */
#define ASSERT_FALSE(cond) FC_TEST_ASSERT(!(cond))

/**
 * @brief Assert NULL
 */
#define ASSERT_NULL(ptr) FC_TEST_ASSERT((ptr) == NULL)

/**
 * @brief Assert not NULL
 */
#define ASSERT_NOT_NULL(ptr) FC_TEST_ASSERT((ptr) != NULL)

#endif /* FC_TEST_FRAMEWORK_H */
