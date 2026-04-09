/**
 * @file fin_kit_cgo.c
 * @brief CGO bindings for fin-kit
 *
 * This file wraps the fin-kit C library functions into a stable ABI
 * suitable for cgo consumption. All functions are thin wrappers around
 * the core library; no business logic lives here.
 *
 * Build modes:
 *   - fin_kit_cgo_source: compile C sources directly (dev mode, requires GCC/Clang)
 *   - prebuilt (default): link against prebuilt static library
 */

#include "fin_kit.h"
#include "platform/simd_detect.h"
#include "platform/mem_aligned.h"

/* ============================================================================
 * Library init / cleanup
 * ============================================================================ */

void fin_kit_lib_init(const fin_kit_config_t* cfg) {
    (void)cfg;
}

void fin_kit_lib_cleanup(void) {
}

/* ============================================================================
 * SIMD detection
 * ============================================================================ */

fin_kit_simd_level_t fin_kit_simd_detect(void) {
    fc_simd_level_t level = fc_detect_simd();
    switch (level) {
        case FC_SIMD_SSE42:  return FIN_KIT_SIMD_SSE42;
        case FC_SIMD_AVX2:   return FIN_KIT_SIMD_AVX2;
        case FC_SIMD_AVX512: return FIN_KIT_SIMD_AVX512;
        default:              return FIN_KIT_SIMD_SCALAR;
    }
}

int fin_kit_simd_parallelism(fin_kit_simd_level_t level) {
    switch (level) {
        case FIN_KIT_SIMD_SSE42:  return 2;
        case FIN_KIT_SIMD_AVX2:   return 4;
        case FIN_KIT_SIMD_AVX512: return 8;
        default:                   return 1;
    }
}

const char* fin_kit_simd_level_string(fin_kit_simd_level_t level) {
    switch (level) {
        case FIN_KIT_SIMD_SCALAR:  return "scalar";
        case FIN_KIT_SIMD_SSE42:   return "SSE4.2";
        case FIN_KIT_SIMD_AVX:     return "AVX";
        case FIN_KIT_SIMD_AVX2:    return "AVX2";
        case FIN_KIT_SIMD_AVX512:  return "AVX-512";
        default:                    return "unknown";
    }
}

/* ============================================================================
 * Aligned memory allocation
 * ============================================================================ */

void* fin_kit_aligned_alloc(size_t size, size_t alignment) {
    extern void* fc_aligned_alloc(size_t size, size_t alignment);
    return fc_aligned_alloc(size, alignment);
}

void fin_kit_aligned_free(void* ptr) {
    extern void fc_aligned_free(void* ptr);
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
 * Status / error strings
 * ============================================================================ */

const char* fin_kit_status_string(int status) {
    switch (status) {
        case FIN_KIT_STATUS_OK:               return "OK";
        case FIN_KIT_STATUS_ERROR:             return "error";
        case FIN_KIT_STATUS_INVALID_POINTER:  return "invalid pointer";
        case FIN_KIT_STATUS_INVALID_SIZE:      return "invalid size";
        case FIN_KIT_STATUS_NO_MEMORY:         return "out of memory";
        case FIN_KIT_STATUS_INVALID_ALIGN:    return "invalid alignment";
        case FIN_KIT_STATUS_NOT_INITIALIZED:   return "not initialized";
        case FIN_KIT_STATUS_ALREADY_DONE:     return "already done";
        case FIN_KIT_STATUS_CANCELLATION:     return "cancelled";
        case FIN_KIT_STATUS_TIMEOUT:           return "timeout";
        case FIN_KIT_STATUS_OVERFLOW:          return "overflow";
        case FIN_KIT_STATUS_UNDERFLOW:         return "underflow";
        case FIN_KIT_STATUS_DIV_BY_ZERO:      return "division by zero";
        case FIN_KIT_STATUS_INVALID_OPERAND:   return "invalid operand";
        case FIN_KIT_STATUS_NOT_IMPLEMENTED:  return "not implemented";
        case FIN_KIT_STATUS_ASSERTION_FAILED:  return "assertion failed";
        default:                                return "unknown status";
    }
}
