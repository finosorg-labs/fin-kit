/**
 * @file fin-kit.h
 * @brief fin-kit public API entry point
 *
 * This is the main public header for the fin-kit library.
 * Include this single header to access all library functionality.
 */

#ifndef FC_FIN_KIT_H
#define FC_FIN_KIT_H

#include <version.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the fin-kit library
 *
 * Detects CPU SIMD capabilities and configures internal state.
 * Must be called before any other library function.
 * Safe to call multiple times; only the first call performs initialization.
 *
 * @return FC_OK on success, FC_ERR_NOT_INITIALIZED on failure
 */
int fc_init(void);

/**
 * @brief Clean up the fin-kit library
 *
 * Releases all resources held by the library.
 * After this call, no other library functions may be called.
 * Safe to call when the library is not initialized (no-op).
 */
void fc_cleanup(void);

/**
 * @brief Get library version as a packed integer
 *
 * @return Version packed as (major << 16) | (minor << 8) | patch
 */
int fc_version(void);

/**
 * @brief Get library version major number
 */
int fc_version_major(void);

/**
 * @brief Get library version minor number
 */
int fc_version_minor(void);

/**
 * @brief Get library version patch number
 */
int fc_version_patch(void);

/**
 * @brief Get library version string (e.g., "1.0.0")
 */
const char* fc_version_string(void);

#ifdef __cplusplus
}
#endif

#endif /* FC_FIN_KIT_H */
