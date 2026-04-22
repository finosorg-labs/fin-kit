/**
 * @file simd_detect.h
 * @brief Runtime SIMD instruction set detection
 *
 * Provides runtime CPU feature detection to automatically identify the highest
 * supported SIMD level, serving as the foundation for runtime dispatch mechanism.
 */

#ifndef FC_SIMD_DETECT_H
#define FC_SIMD_DETECT_H

#include "platform.h"

FC_BEGIN_DECLS

/**
 * @brief SIMD instruction set level enumeration
 *
 * Ordered by capability from lowest to highest. Runtime detection determines
 * the highest level supported by the system.
 */
typedef enum {
    FC_SIMD_SCALAR  = 0,  /**< Scalar operations only (no SIMD) */
    FC_SIMD_SSE42   = 1,  /**< SSE4.2 supported */
    FC_SIMD_AVX2    = 2,  /**< AVX2 supported */
    FC_SIMD_AVX512  = 3,  /**< AVX-512 supported */
    FC_SIMD_NEON    = 4,  /**< ARM NEON supported */
} fc_simd_level_t;

/**
 * @brief Detect the highest SIMD level supported by the CPU
 *
 * Uses CPUID instruction to detect SIMD instruction sets supported by the
 * target system. This function should be called once during library
 * initialization; the result can then be safely read by all threads.
 *
 * Detection priority: AVX-512 > AVX2 > SSE4.2 > Scalar
 *
 * @return Highest supported SIMD level
 *
 * @note Thread-safe: no locking required after result is written to global
 *       read-only variable
 */
fc_simd_level_t fc_detect_simd(void);

/**
 * @brief Get the current SIMD level
 *
 * Returns the SIMD level after initialization by fc_detect_simd().
 *
 * @return Current SIMD level
 */
fc_simd_level_t fc_get_simd_level(void);

/**
 * @brief Global SIMD level variable (read-only after initialization)
 */
extern fc_simd_level_t g_fc_simd_level;

fc_simd_level_t fc_simd_detect_unsafe(void);

/**
 * @brief Get string description of SIMD level
 *
 * @param level SIMD level
 * @return Level description string
 */
const char* fc_simd_level_string(fc_simd_level_t level);

/**
 * @brief Get SIMD parallelism (number of doubles processable in parallel)
 *
 * @param level SIMD level
 * @return Number of doubles that can be processed in parallel
 */
size_t fc_simd_parallelism(fc_simd_level_t level);

FC_END_DECLS

#endif /* FC_SIMD_DETECT_H */
