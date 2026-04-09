/**
 * @file fin_kit.h
 * @brief Public C API header for fin-kit library
 *
 * This header defines the stable C ABI exposed to Go via cgo.
 * It wraps the internal platform layer (simd_detect, mem_aligned) into
 * a versioned, forward-compatible interface.
 */

#ifndef FIN_KIT_H
#define FIN_KIT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * API version
 * ============================================================================ */

#define FIN_KIT_VERSION_MAJOR 1
#define FIN_KIT_VERSION_MINOR 0
#define FIN_KIT_VERSION_PATCH 0

/* ============================================================================
 * SIMD level enumeration (stable ABI)
 * Must match Go fin_kit.SIMDLevel values.
 * ============================================================================ */

typedef enum {
    FIN_KIT_SIMD_SCALAR = 0,
    FIN_KIT_SIMD_SSE42  = 1,
    FIN_KIT_SIMD_AVX    = 2,
    FIN_KIT_SIMD_AVX2   = 3,
    FIN_KIT_SIMD_AVX512 = 4,
} fin_kit_simd_level_t;

/* ============================================================================
 * Status / error codes
 * Must match Go fin_kit.Status values.
 * ============================================================================ */

typedef enum {
    FIN_KIT_STATUS_OK                = 0,
    FIN_KIT_STATUS_ERROR             = 1,
    FIN_KIT_STATUS_INVALID_POINTER  = 2,
    FIN_KIT_STATUS_INVALID_SIZE     = 3,
    FIN_KIT_STATUS_NO_MEMORY        = 4,
    FIN_KIT_STATUS_INVALID_ALIGN    = 5,
    FIN_KIT_STATUS_NOT_INITIALIZED  = 6,
    FIN_KIT_STATUS_ALREADY_DONE     = 7,
    FIN_KIT_STATUS_CANCELLATION     = 8,
    FIN_KIT_STATUS_TIMEOUT          = 9,
    FIN_KIT_STATUS_OVERFLOW         = 10,
    FIN_KIT_STATUS_UNDERFLOW        = 11,
    FIN_KIT_STATUS_DIV_BY_ZERO      = 12,
    FIN_KIT_STATUS_INVALID_OPERAND  = 13,
    FIN_KIT_STATUS_NOT_IMPLEMENTED  = 14,
    FIN_KIT_STATUS_ASSERTION_FAILED = 15,
} fin_kit_status_t;

/* ============================================================================
 * Library configuration
 * ============================================================================ */

typedef struct {
    int      num_threads;     /* 0 = auto (use all CPUs) */
    int      enable_avx2;    /* force AVX2 even if higher available */
    unsigned memory_limit_mb; /* 0 = unlimited */
    int      verbose;        /* enable diagnostic output */
} fin_kit_config_t;

/* ============================================================================
 * Library init / cleanup
 * ============================================================================ */

/**
 * @brief Initialize the fin-kit library.
 * @param cfg Configuration (can be NULL for defaults).
 */
void fin_kit_lib_init(const fin_kit_config_t* cfg);

/**
 * @brief Cleanup and release all library resources.
 * Must be called once per successful fin_kit_lib_init().
 */
void fin_kit_lib_cleanup(void);

/* ============================================================================
 * SIMD detection
 * ============================================================================ */

/**
 * @brief Detect the highest SIMD level supported by the current CPU.
 * @return SIMD level (0=scalar, 1=SSE4.2, 2=AVX, 3=AVX2, 4=AVX-512).
 */
fin_kit_simd_level_t fin_kit_simd_detect(void);

/**
 * @brief Get a human-readable string for a SIMD level.
 * @param level SIMD level value.
 * @return Static string, never NULL.
 */
const char* fin_kit_simd_level_string(fin_kit_simd_level_t level);

/**
 * @brief Get the SIMD parallelism (number of doubles per vector op).
 * @param level SIMD level.
 * @return Number of doubles processable per SIMD instruction.
 */
int fin_kit_simd_parallelism(fin_kit_simd_level_t level);

/* ============================================================================
 * Aligned memory allocation
 * ============================================================================ */

/**
 * @brief Allocate memory with the given alignment.
 * @param size  Number of bytes to allocate.
 * @param alignment Alignment in bytes (must be power of 2).
 * @return Pointer to allocated memory, or NULL on failure.
 * @note Use fin_kit_aligned_free() to release.
 */
void* fin_kit_aligned_alloc(size_t size, size_t alignment);

/**
 * @brief Free memory allocated by fin_kit_aligned_alloc().
 * @param ptr Pointer returned by fin_kit_aligned_alloc().
 */
void fin_kit_aligned_free(void* ptr);

/* ============================================================================
 * Platform info
 * ============================================================================ */

/**
 * @brief Get the compiler name.
 * @return "gcc", "clang", "msvc", or "unknown".
 */
const char* fin_kit_compiler_name(void);

/**
 * @brief Get the compiler version string.
 */
const char* fin_kit_compiler_version(void);

/**
 * @brief Get the operating system name.
 * @return "linux", "windows", "darwin", or "unknown".
 */
const char* fin_kit_os_name(void);

/**
 * @brief Get the CPU architecture name.
 * @return "x86_64", "x86", "aarch64", "arm", or "unknown".
 */
const char* fin_kit_arch_name(void);

/* ============================================================================
 * Status / error utilities
 * ============================================================================ */

/**
 * @brief Get a human-readable string for a status code.
 * @param status Status code value.
 * @return Static string, never NULL.
 */
const char* fin_kit_status_string(int status);

#ifdef __cplusplus
}
#endif

#endif /* FIN_KIT_H */
