/**
 * @file mem_aligned.c
 * @brief SIMD-aligned memory allocation implementation
 *
 * Provides cross-platform aligned memory allocation.
 * Supports Windows, Linux, and macOS platform APIs.
 */

#include "mem_aligned.h"
#include "simd_detect.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ============================================================================
 * Platform-specific aligned memory allocation
 * ============================================================================ */

#if defined(FC_OS_WINDOWS)

#include <malloc.h>

void* fc_aligned_alloc(size_t size, size_t alignment) {
    if (size == 0) {
        return NULL;
    }

    /* Windows _aligned_malloc requires alignment >= sizeof(void*) */
    if (alignment < sizeof(void*)) {
        alignment = sizeof(void*);
    }

    /* Alignment must be power of 2 */
    if ((alignment & (alignment - 1)) != 0) {
        alignment = (size_t)1 << (64 - __builtin_clzll(alignment));
    }

    return _aligned_malloc(size, alignment);
}

void fc_aligned_free(void* ptr) {
    if (ptr != NULL) {
        _aligned_free(ptr);
    }
}

#elif defined(FC_OS_LINUX) || defined(FC_OS_MACOS)

#include <stdlib.h>

void* fc_aligned_alloc(size_t size, size_t alignment) {
    void* ptr = NULL;
    int result;

    if (size == 0) {
        return NULL;
    }

    /* posix_memalign requires alignment >= sizeof(void*) and power of 2 */
    if (alignment < sizeof(void*)) {
        alignment = sizeof(void*);
    }

    if ((alignment & (alignment - 1)) != 0) {
        return NULL;
    }

    /* Check for integer overflow */
    if (size > ((size_t)-1) - alignment) {
        return NULL;
    }

    result = posix_memalign(&ptr, alignment, size);
    if (result != 0) {
        return NULL;
    }

    return ptr;
}

void fc_aligned_free(void* ptr) {
    free(ptr);
}

#else

#warning "Unknown platform, using standard malloc (may not satisfy alignment requirements)"

void* fc_aligned_alloc(size_t size, size_t alignment) {
    if (size == 0) {
        return NULL;
    }

    if ((alignment & (alignment - 1)) != 0) {
        return NULL;
    }

    /* Allocate extra space to store original pointer for free() */
    void* raw = malloc(size + alignment);
    if (raw == NULL) {
        return NULL;
    }

    /* Align pointer upward */
    uintptr_t addr = (uintptr_t)raw;
    addr = (addr + alignment - 1) & ~(alignment - 1);

    /* Store original pointer before aligned pointer for recovery in free() */
    void** aligned_ptr = (void**)addr;
    aligned_ptr[-1] = raw;

    return (void*)aligned_ptr;
}

void fc_aligned_free(void* ptr) {
    if (ptr != NULL) {
        void** aligned_ptr = (void**)ptr;
        free(aligned_ptr[-1]);
    }
}

#endif /* FC_OS_* */

/* ============================================================================
 * Memory alignment utility functions
 * ============================================================================ */

int fc_is_aligned(const void* ptr, size_t alignment) {
    if (ptr == NULL) {
        return 1;
    }

    if ((alignment & (alignment - 1)) != 0) {
        return 0;
    }

    uintptr_t addr = (uintptr_t)ptr;
    return (addr & (alignment - 1)) == 0;
}

size_t fc_get_default_alignment(void) {
    fc_simd_level_t level = fc_get_simd_level();

    switch (level) {
        case FC_SIMD_AVX512:
            return FC_ALIGNMENT_AVX512;
        case FC_SIMD_AVX2:
            return FC_ALIGNMENT_AVX2;
        case FC_SIMD_SSE42:
            return FC_ALIGNMENT_SSE42;
        default:
            return FC_DEFAULT_ALIGNMENT;
    }
}

size_t fc_align_size(size_t size, size_t alignment) {
    if (size == 0) {
        return 0;
    }

    /* Round up to nearest power of 2 if not already */
    if ((alignment & (alignment - 1)) != 0) {
        alignment = (size_t)1 << (64 - __builtin_clzll(alignment));
    }

    return (size + alignment - 1) & ~(alignment - 1);
}

void* fc_align_ptr(const void* ptr, size_t alignment) {
    uintptr_t addr = (uintptr_t)ptr;

    if ((alignment & (alignment - 1)) != 0) {
        return (void*)ptr;
    }

    addr = (addr + alignment - 1) & ~(alignment - 1);
    return (void*)addr;
}

/* ============================================================================
 * Typed allocation convenience functions
 * ============================================================================ */

void* fc_aligned_alloc_double(size_t n) {
    return fc_aligned_alloc(n * sizeof(double), fc_get_default_alignment());
}

void* fc_aligned_alloc_float(size_t n) {
    return fc_aligned_alloc(n * sizeof(float), fc_get_default_alignment());
}

void* fc_aligned_alloc_int64(size_t n) {
    return fc_aligned_alloc(n * sizeof(int64_t), fc_get_default_alignment());
}

void* fc_aligned_alloc_int32(size_t n) {
    return fc_aligned_alloc(n * sizeof(int32_t), fc_get_default_alignment());
}

void* fc_aligned_alloc_uint64(size_t n) {
    return fc_aligned_alloc(n * sizeof(uint64_t), fc_get_default_alignment());
}

void* fc_aligned_alloc_uint32(size_t n) {
    return fc_aligned_alloc(n * sizeof(uint32_t), fc_get_default_alignment());
}
