/**
 * @file fin_kit_source.c.h
 * @brief C source wrapper for Go cgo source mode
 *
 * This file implements the stable C ABI defined in fin_kit.h by wrapping
 * the internal platform layer functions. It is included directly by
 * fin_kit_cgo_source.go when building with -tags fin_kit_cgo.
 */

#ifndef FIN_KIT_SOURCE_C_H
#define FIN_KIT_SOURCE_C_H

#include "fin_kit.h"
#include "../../src/platform/platform.h"
#include "../../src/platform/simd_detect.h"
#include "../../src/platform/mem_aligned.h"
#include "../../src/platform/error.h"

/* ============================================================================
 * Library initialization
 * ============================================================================ */

void fin_kit_lib_init(const fin_kit_config_t* cfg) {
    (void)cfg; /* Configuration not yet used in Stage 0 */
    fc_init();
}

void fin_kit_lib_cleanup(void) {
    fc_cleanup();
}

/* ============================================================================
 * SIMD detection
 * ============================================================================ */

fin_kit_simd_level_t fin_kit_simd_detect(void) {
    fc_simd_level_t level = fc_detect_simd();
    return (fin_kit_simd_level_t)level;
}

const char* fin_kit_simd_level_string(fin_kit_simd_level_t level) {
    return fc_simd_level_string((fc_simd_level_t)level);
}

int fin_kit_simd_parallelism(fin_kit_simd_level_t level) {
    return (int)fc_simd_parallelism((fc_simd_level_t)level);
}

/* ============================================================================
 * Aligned memory allocation
 * ============================================================================ */

void* fin_kit_aligned_alloc(size_t size, size_t alignment) {
    return fc_aligned_alloc(size, alignment);
}

void fin_kit_aligned_free(void* ptr) {
    fc_aligned_free(ptr);
}

/* ============================================================================
 * Platform info
 * ============================================================================ */

const char* fin_kit_compiler_name(void) {
#if FC_COMPILER_CLANG
    return "clang";
#elif FC_COMPILER_GCC
    return "gcc";
#else
    return "unknown";
#endif
}

const char* fin_kit_compiler_version(void) {
#if FC_COMPILER_GCC || FC_COMPILER_CLANG
    return __VERSION__;
#else
    return "unknown";
#endif
}

const char* fin_kit_os_name(void) {
    return FC_OS_STRING;
}

const char* fin_kit_arch_name(void) {
    return FC_ARCH_STRING;
}

/* ============================================================================
 * Status / error utilities
 * ============================================================================ */

const char* fin_kit_status_string(int status) {
    return fc_status_string((fc_status_t)status);
}

#endif /* FIN_KIT_SOURCE_C_H */
