/**
 * @file mem_aligned.h
 * @brief SIMD-aligned memory allocation
 *
 * Provides aligned memory allocation interfaces required for SIMD operations.
 * AVX2 requires 32-byte alignment, AVX-512 requires 64-byte alignment.
 */

#ifndef FC_MEM_ALIGNED_H
#define FC_MEM_ALIGNED_H

#include "platform.h"
#include <stddef.h>

FC_BEGIN_DECLS

/**
 * @brief Default alignment in bytes (matches AVX-512)
 */
#define FC_DEFAULT_ALIGNMENT 64

/**
 * @brief Common alignment values
 */
#define FC_ALIGNMENT_SSE42  16
#define FC_ALIGNMENT_AVX2   32
#define FC_ALIGNMENT_AVX512 64

/**
 * @brief Allocate aligned memory
 *
 * Allocates memory of the specified size with the given alignment requirement.
 *
 * @param size Number of bytes to allocate
 * @param alignment Alignment requirement (must be power of 2)
 *
 * @return Pointer to allocated memory, NULL on failure
 *
 * @note Caller is responsible for freeing memory with fc_aligned_free()
 * @note Memory is uninitialized, contents are indeterminate
 */
void* fc_aligned_alloc(size_t size, size_t alignment);

/**
 * @brief Free aligned memory
 *
 * Frees memory allocated by fc_aligned_alloc().
 *
 * @param ptr Pointer returned by previous fc_aligned_alloc() call
 */
void fc_aligned_free(void* ptr);

/**
 * @brief Check if pointer satisfies alignment requirement
 *
 * @param ptr Pointer to check
 * @param alignment Alignment requirement (must be power of 2)
 *
 * @return 1 if aligned correctly, 0 if not aligned
 */
int fc_is_aligned(const void* ptr, size_t alignment);

/**
 * @brief Get default alignment in bytes
 *
 * @return Default alignment based on runtime-detected SIMD level
 */
size_t fc_get_default_alignment(void);

/**
 * @brief Calculate aligned size
 *
 * Rounds up the given size to the specified alignment boundary.
 *
 * @param size Original size
 * @param alignment Alignment requirement
 *
 * @return Aligned size
 */
size_t fc_align_size(size_t size, size_t alignment);

/**
 * @brief Calculate aligned pointer
 *
 * Rounds up the given pointer to the specified alignment boundary.
 *
 * @param ptr Original pointer
 * @param alignment Alignment requirement
 *
 * @return Aligned pointer
 */
void* fc_align_ptr(const void* ptr, size_t alignment);

/**
 * @brief Allocate SIMD-aligned double array
 *
 * Convenience function to allocate aligned memory for n doubles.
 *
 * @param n Number of elements
 *
 * @return Pointer to allocated memory, NULL on failure
 *
 * @note Uses fc_get_default_alignment() for alignment
 */
void* fc_aligned_alloc_double(size_t n);

/**
 * @brief Allocate SIMD-aligned float array
 *
 * Convenience function to allocate aligned memory for n floats.
 *
 * @param n Number of elements
 *
 * @return Pointer to allocated memory, NULL on failure
 */
void* fc_aligned_alloc_float(size_t n);

/**
 * @brief Allocate SIMD-aligned int64 array
 *
 * Convenience function to allocate aligned memory for n int64_t values.
 *
 * @param n Number of elements
 *
 * @return Pointer to allocated memory, NULL on failure
 */
void* fc_aligned_alloc_int64(size_t n);

/**
 * @brief Allocate SIMD-aligned int32 array
 *
 * Convenience function to allocate aligned memory for n int32_t values.
 *
 * @param n Number of elements
 *
 * @return Pointer to allocated memory, NULL on failure
 */
void* fc_aligned_alloc_int32(size_t n);

/**
 * @brief Allocate SIMD-aligned uint64 array
 *
 * Convenience function to allocate aligned memory for n uint64_t values.
 *
 * @param n Number of elements
 *
 * @return Pointer to allocated memory, NULL on failure
 */
void* fc_aligned_alloc_uint64(size_t n);

/**
 * @brief Allocate SIMD-aligned uint32 array
 *
 * Convenience function to allocate aligned memory for n uint32_t values.
 *
 * @param n Number of elements
 *
 * @return Pointer to allocated memory, NULL on failure
 */
void* fc_aligned_alloc_uint32(size_t n);

FC_END_DECLS

#endif /* FC_MEM_ALIGNED_H */
