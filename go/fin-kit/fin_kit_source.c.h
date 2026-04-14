/**
 * @file fin_kit_source.c.h
 * @brief CGO bindings for fin-kit (source mode)
 *
 * This file wraps the fin-kit C library functions into a stable ABI
 * suitable for cgo consumption. In source mode, C sources are compiled
 * directly alongside this wrapper.
 *
 * Build mode: go build -tags fin_kit_cgo
 */

#include "fin_kit.h"
#include "../../src/platform/simd_detect.h"
#include "../../src/platform/mem_aligned.h"
#include "../../src/platform/error.h"

/* ============================================================================
 * Library init / cleanup — delegates to C core API
 * ============================================================================ */

void fin_kit_lib_init(const fin_kit_config_t* cfg) {
    (void)cfg;
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
    switch (level) {
        case FC_SIMD_SCALAR:  return FIN_KIT_SIMD_SCALAR;
        case FC_SIMD_SSE42:   return FIN_KIT_SIMD_SSE42;
        case FC_SIMD_AVX2:    return FIN_KIT_SIMD_AVX2;
        case FC_SIMD_AVX512:  return FIN_KIT_SIMD_AVX512;
        case FC_SIMD_NEON:    return FIN_KIT_SIMD_NEON;
        default:              return FIN_KIT_SIMD_SCALAR;
    }
}

int fin_kit_simd_parallelism(fin_kit_simd_level_t level) {
    switch (level) {
        case FIN_KIT_SIMD_SSE42:   return 2;
        case FIN_KIT_SIMD_AVX2:    return 4;
        case FIN_KIT_SIMD_AVX512:  return 8;
        case FIN_KIT_SIMD_NEON:    return 2;
        default:                   return 1;
    }
}

const char* fin_kit_simd_level_string(fin_kit_simd_level_t level) {
    switch (level) {
        case FIN_KIT_SIMD_SCALAR:  return "Scalar";
        case FIN_KIT_SIMD_SSE42:   return "SSE4.2";
        case FIN_KIT_SIMD_AVX2:    return "AVX2";
        case FIN_KIT_SIMD_AVX512:  return "AVX-512";
        case FIN_KIT_SIMD_NEON:    return "NEON";
        default:                    return "Unknown";
    }
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
#if defined(__clang__)
    return "clang";
#elif defined(__GNUC__)
    return "gcc";
#elif defined(_MSC_VER)
    return "msvc";
#else
    return "unknown";
#endif
}

const char* fin_kit_compiler_version(void) {
#if defined(__clang__)
    return __clang_version__;
#elif defined(__GNUC__)
    return __VERSION__;
#elif defined(_MSC_VER)
    return "MSVC";
#else
    return "unknown";
#endif
}

const char* fin_kit_os_name(void) {
#if defined(_WIN32)
    return "windows";
#elif defined(__linux__)
    return "linux";
#elif defined(__APPLE__)
    return "darwin";
#elif defined(__FreeBSD__)
    return "freebsd";
#else
    return "unknown";
#endif
}

const char* fin_kit_arch_name(void) {
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "aarch64";
#elif defined(__arm__) || defined(_M_ARM)
    return "arm";
#else
    return "unknown";
#endif
}

/* ============================================================================
 * Version — delegates to C core API
 * ============================================================================ */

int fin_kit_version(void) {
    return fc_version();
}

int fin_kit_version_major(void) {
    return fc_version_major();
}

int fin_kit_version_minor(void) {
    return fc_version_minor();
}

int fin_kit_version_patch(void) {
    return fc_version_patch();
}

const char* fin_kit_version_string(void) {
    return fc_version_string();
}

/* ============================================================================
 * Status / error strings
 * ============================================================================ */

const char* fin_kit_status_string(int status) {
    return fc_status_string((fc_status_t)status);
}