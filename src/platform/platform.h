/**
 * @file platform.h
 * @brief Platform abstraction layer core header
 *
 * Defines cross-platform compatible base types, macros, and compiler abstractions.
 * All other headers should include this file for platform-independent base definitions.
 *
 * Main features:
 * - Compiler detection (GCC, Clang)
 * - Architecture detection (x86, x86_64, ARM, ARM64)
 * - Operating system detection (Windows, Linux, macOS)
 * - Export/import macro definitions
 * - Base type aliases
 * - Common utility macros
 * - Atomic operations and memory barriers
 */

#ifndef FC_PLATFORM_H
#define FC_PLATFORM_H

/*
 * Compiler detection
*/

#if defined(__GNUC__) || defined(__clang__)
    #define FC_COMPILER_GCC   1
    #define FC_COMPILER_CLANG (__clang__)
    #define FC_COMPILER_VERSION_MAJOR __GNUC__
    #define FC_COMPILER_VERSION_MINOR __GNUC_MINOR__
    #define FC_GCC_VERSION     (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#else
    #warning "Unknown compiler, using defaults"
    #define FC_COMPILER_GCC   0
    #define FC_COMPILER_CLANG 0
#endif

/* C standard version */
#if defined(__STDC_VERSION__)
    #define FC_C_VERSION __STDC_VERSION__
#else
    #define FC_C_VERSION 199901L
#endif

/*
 * Architecture detection
*/

#if defined(__x86_64__)
    #define FC_ARCH_X86_64   1
    #define FC_ARCH_X86      0
    #define FC_ARCH_ARM64    0
    #define FC_ARCH_ARM      0
    #define FC_ARCH_GENERIC  0
    #define FC_ARCH_STRING   "x86_64"
#elif defined(__i386__)
    #define FC_ARCH_X86_64   0
    #define FC_ARCH_X86      1
    #define FC_ARCH_ARM64    0
    #define FC_ARCH_ARM      0
    #define FC_ARCH_GENERIC  0
    #define FC_ARCH_STRING   "x86"
#elif defined(__aarch64__)
    #define FC_ARCH_X86_64   0
    #define FC_ARCH_X86      0
    #define FC_ARCH_ARM64    1
    #define FC_ARCH_ARM      0
    #define FC_ARCH_GENERIC  0
    #define FC_ARCH_STRING   "arm64"
#elif defined(__arm__)
    #define FC_ARCH_X86_64   0
    #define FC_ARCH_X86      0
    #define FC_ARCH_ARM64    0
    #define FC_ARCH_ARM      1
    #define FC_ARCH_GENERIC  0
    #define FC_ARCH_STRING   "arm"
#else
    #define FC_ARCH_X86_64   0
    #define FC_ARCH_X86      0
    #define FC_ARCH_ARM64    0
    #define FC_ARCH_ARM      0
    #define FC_ARCH_GENERIC  1
    #define FC_ARCH_STRING   "generic"
#endif

/*
 * Operating system detection
*/

/* FC_OS_* defaults (may be overridden by toolchain -D flags) */
#ifndef FC_OS_WINDOWS
    #if defined(_WIN32)
        #define FC_OS_WINDOWS  1
    #else
        #define FC_OS_WINDOWS  0
    #endif
#endif

#ifndef FC_OS_LINUX
    #if defined(__linux__)
        #define FC_OS_LINUX   1
    #else
        #define FC_OS_LINUX   0
    #endif
#endif

#ifndef FC_OS_MACOS
    #if defined(__APPLE__) && defined(__MACH__)
        #define FC_OS_MACOS   1
    #else
        #define FC_OS_MACOS   0
    #endif
#endif

/* Derive FC_OS_STRING from the resolved values */
#if FC_OS_WINDOWS
    #define FC_OS_STRING   "Windows"
#elif FC_OS_LINUX
    #define FC_OS_STRING   "Linux"
#elif FC_OS_MACOS
    #define FC_OS_STRING   "macOS"
#else
    #define FC_OS_STRING   "Unknown"
#endif

/*
 * SIMD instruction support detection (compile-time)
*/

/* SSE4.2 */
#if defined(__SSE4_2__)
    #define FC_HAS_SSE42 1
#else
    #define FC_HAS_SSE42 0
#endif

/* AVX */
#if defined(__AVX__)
    #define FC_HAS_AVX 1
#else
    #define FC_HAS_AVX 0
#endif

/* AVX2 */
#if defined(__AVX2__)
    #define FC_HAS_AVX2 1
#else
    #define FC_HAS_AVX2 0
#endif

/* AVX-512 */
#if defined(__AVX512F__)
    #define FC_HAS_AVX512 1
#else
    #define FC_HAS_AVX512 0
#endif

/* FMA support */
#if defined(__FMA__)
    #define FC_HAS_FMA 1
#else
    #define FC_HAS_FMA 0
#endif

/* NEON support (ARM) */
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    #define FC_HAS_NEON 1
#else
    #define FC_HAS_NEON 0
#endif

/*
 * Export/import macros (ABI compatibility)
*/

#if defined(FC_STATIC_BUILD)
    #define FC_API
    #define FC_INTERNAL
#else
    #if defined(_WIN32)
        #ifdef FC_EXPORTS
            #define FC_API __attribute__((dllexport))
        #else
            #define FC_API __attribute__((dllimport))
        #endif
        #define FC_INTERNAL
    #else
        #define FC_API __attribute__((visibility("default")))
        #define FC_INTERNAL __attribute__((visibility("hidden")))
    #endif
#endif

/*
 * Base type aliases
*/

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Signed integer types */
typedef int8_t    fc_int8_t;
typedef int16_t   fc_int16_t;
typedef int32_t   fc_int32_t;
typedef int64_t   fc_int64_t;

/* Unsigned integer types */
typedef uint8_t   fc_uint8_t;
typedef uint16_t  fc_uint16_t;
typedef uint32_t  fc_uint32_t;
typedef uint64_t  fc_uint64_t;

/* Boolean type */
typedef int       fc_bool_t;

/* Size and pointer difference types */
typedef size_t    fc_size_t;
typedef ptrdiff_t fc_ptrdiff_t;

/* Floating-point types */
typedef float     fc_float_t;
typedef double    fc_double_t;

/*
 * Function declaration begin/end macros (for C++ compatibility)
*/

#ifdef __cplusplus
    #define FC_BEGIN_DECLS extern "C" {
    #define FC_END_DECLS   }
#else
    #define FC_BEGIN_DECLS
    #define FC_END_DECLS
#endif

/*
 * Memory alignment static assertions (C11)
*/

_Static_assert(sizeof(int) >= 4, "fc_status_t requires at least 32-bit integer");

/*
 * Common utility macros
*/

/* Null pointer */
#define FC_NULL ((void*)0)

/* Array length */
#define FC_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/* Swap two values */
#define FC_SWAP(a, b, type) do { \
    type _tmp = (a); \
    (a) = (b); \
    (b) = _tmp; \
} while (0)

/* Min/max */
#define FC_MIN(a, b) ((a) < (b) ? (a) : (b))
#define FC_MAX(a, b) ((a) > (b) ? (a) : (b))
#define FC_CLAMP(val, lo, hi) FC_MAX((lo), FC_MIN((val), (hi)))

/* Power of 2 operations */
#define FC_IS_POWER_OF_2(x) (((x) & ((x) - 1)) == 0)
#define FC_ROUND_UP_TO_POW2(x) do { \
    (x)--; \
    (x) |= (x) >> 1; \
    (x) |= (x) >> 2; \
    (x) |= (x) >> 4; \
    (x) |= (x) >> 8; \
    (x) |= (x) >> 16; \
    if (sizeof(x) == 8) (x) |= (x) >> 32; \
    (x)++; \
} while (0)

/* Byte size helpers */
#define FC_KB(x) ((size_t)(x) * 1024)
#define FC_MB(x) ((size_t)(x) * 1024 * 1024)
#define FC_GB(x) ((size_t)(x) * 1024 * 1024 * 1024)

/*
 * Debug assertions
*/

#if defined(FC_DEBUG) || defined(_DEBUG)
    #define FC_DEBUG_MODE 1
#else
    #define FC_DEBUG_MODE 0
#endif

#if FC_DEBUG_MODE
    #include <stdio.h>
    #define FC_ASSERT(cond, msg) do { \
        if (!(cond)) { \
            fprintf(stderr, "Assertion failed: %s, file %s, line %d\n", \
                    (msg), __FILE__, __LINE__); \
        } \
    } while (0)
#else
    #define FC_ASSERT(cond, msg) ((void)0)
#endif

#define FC_ASSERT_ALWAYS(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "Critical assertion failed: %s, file %s, line %d\n", \
                (msg), __FILE__, __LINE__); \
    } \
} while (0)

/*
 * Compiler-specific hints
*/

/* Branch prediction hints */
#if defined(__GNUC__) || defined(__clang__)
    #define FC_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define FC_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define FC_LIKELY(x)   (x)
    #define FC_UNLIKELY(x) (x)
#endif

/* Pure function hints (same input produces same output, no side effects) */
#if defined(__GNUC__) || defined(__clang__)
    #define FC_PURE   __attribute__((pure))
    #define FC_CONST  __attribute__((const))
#else
    #define FC_PURE
    #define FC_CONST
#endif

/* Inline hints */
#if defined(__GNUC__) || defined(__clang__)
    #define FC_INLINE inline __attribute__((always_inline))
#else
    #define FC_INLINE inline
#endif

#define FC_LIKELY_ALWAYS(x)   (x)
#define FC_UNLIKELY_ALWAYS(x) (x)

/* Function noreturn annotation */
#if defined(__GNUC__) || defined(__clang__)
    #define FC_NORETURN __attribute__((noreturn))
#else
    #define FC_NORETURN
#endif

/* Function possibly unused annotation */
#if defined(__GNUC__) || defined(__clang__)
    #define FC_UNUSED __attribute__((unused))
#else
    #define FC_UNUSED
#endif

/* Struct alignment */
#if defined(__GNUC__) || defined(__clang__)
    #define FC_ALIGNED(n) __attribute__((aligned(n)))
#else
    #define FC_ALIGNED(n)
#endif

/* Packed struct (no padding) */
#if defined(__GNUC__) || defined(__clang__)
    #define FC_PACKED __attribute__((packed))
#else
    #define FC_PACKED
#endif

/*
 * Memory barriers and atomic operations
*/

#if defined(__GNUC__) || defined(__clang__)
    #define FC_MEMORY_BARRIER() __asm__ volatile("" ::: "memory")
#else
    #define FC_MEMORY_BARRIER()
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define FC_COMPILER_BARRIER() __asm__ volatile("" ::: "memory")
#else
    #define FC_COMPILER_BARRIER()
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define FC_ATOMIC_LOAD(ptr) __atomic_load_n((ptr), __ATOMIC_ACQUIRE)
    #define FC_ATOMIC_STORE(ptr, val) __atomic_store_n((ptr), (val), __ATOMIC_RELEASE)
#else
    #define FC_ATOMIC_LOAD(ptr) (*(ptr))
    #define FC_ATOMIC_STORE(ptr, val) ((*(ptr)) = (val))
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define FC_ATOMIC_CAS(ptr, old_val, new_val) \
        __atomic_compare_exchange_n((ptr), &(old_val), (new_val), 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
#else
    #define FC_ATOMIC_CAS(ptr, old_val, new_val) \
        ((*(ptr) == (old_val)) ? ((*(ptr) = (new_val)), 1) : 0)
#endif

/*
 * Floating-point control (IEEE 754 guarantees, enabled by default)
*/

#if defined(__GNUC__) || defined(__clang__)
    #define FC_FP_CONTRACT_OFF _Pragma("STDC FP_CONTRACT OFF")
#else
    #define FC_FP_CONTRACT_OFF
#endif

/* Floating-point rounding modes (if needed) */
#define FC_FP_ROUND_NEAREST       0
#define FC_FP_ROUND_TOWARD_ZERO   1
#define FC_FP_ROUND_UP            2
#define FC_FP_ROUND_DOWN          3

/*
 * Platform-specific optimizations and adjustments
*/

#if FC_ARCH_X86 || FC_ARCH_X86_64
    /* Cache line size (for false sharing avoidance) */
    #define FC_CACHE_LINE_SIZE 64

    /* Allow unaligned SIMD access (faster than throwing exceptions) */
    #if defined(__GNUC__) || defined(__clang__)
        #pragma GCC diagnostic ignored "-Wcast-align"
    #endif
#endif

#if FC_ARCH_ARM || FC_ARCH_ARM64
    #define FC_CACHE_LINE_SIZE 64
#endif

#ifndef FC_CACHE_LINE_SIZE
    #define FC_CACHE_LINE_SIZE 64
#endif

/*
 * Miscellaneous utility function declarations
*/

FC_BEGIN_DECLS

/**
 * @brief Get system cache line size
 *
 * @return Cache line size in bytes
 */
FC_API size_t fc_get_cache_line_size(void);

/**
 * @brief Get L1 data cache size
 *
 * @return L1 data cache size in bytes, 0 on failure
 */
FC_API size_t fc_get_l1_cache_size(void);

/**
 * @brief Get L2 cache size
 *
 * @return L2 cache size in bytes, 0 on failure
 */
FC_API size_t fc_get_l2_cache_size(void);

/**
 * @brief Get L3 cache size
 *
 * @return L3 cache size in bytes, 0 on failure
 */
FC_API size_t fc_get_l3_cache_size(void);

FC_END_DECLS

/*
 * Version information (primary source: cmake/version.h.in, fallback below)
*/

/* Include generated version.h if available */
#if defined(FC_HAS_VERSION_HEADER)
#include "version.h"
#else

#ifndef FC_VERSION_MAJOR
#define FC_VERSION_MAJOR  1
#endif
#ifndef FC_VERSION_MINOR
#define FC_VERSION_MINOR  0
#endif
#ifndef FC_VERSION_PATCH
#define FC_VERSION_PATCH  0
#endif
#ifndef FC_VERSION_STRING
#define FC_VERSION_STRING "1.0.0"
#endif

#endif /* FC_HAS_VERSION_HEADER */

#endif /* FC_PLATFORM_H */
