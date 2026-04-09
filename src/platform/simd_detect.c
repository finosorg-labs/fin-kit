/**
 * @file simd_detect.c
 * @brief Runtime SIMD instruction set detection implementation
 *
 * Detects CPU SIMD capabilities via CPUID instruction.
 * Supports x86/x86_64 architecture on Linux, Windows, and macOS.
 */

#include "simd_detect.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>

/* Global SIMD level variable (read-only after initialization, thread-safe) */
fc_simd_level_t g_fc_simd_level = FC_SIMD_SCALAR;

/* ============================================================================
 * Platform-specific CPUID detection implementation
 * ============================================================================ */

#if defined(FC_ARCH_X86) || defined(FC_ARCH_X86_64)

/* x86/x86_64 platform: using inline assembly or intrinsics */

/**
 * @brief Execute CPUID instruction
 *
 * @param eax Input EAX value
 * @param ecx Input ECX value (required for certain leaves)
 * @param regs Output register array [eax, ebx, ecx, edx]
 */
static inline void fc_cpuid(uint32_t eax, uint32_t ecx, uint32_t regs[4]) {
#if defined(FC_COMPILER_MSVC)
    int info[4];
    __cpuidex(info, (int)eax, (int)ecx);
    regs[0] = (uint32_t)info[0];
    regs[1] = (uint32_t)info[1];
    regs[2] = (uint32_t)info[2];
    regs[3] = (uint32_t)info[3];
#elif defined(FC_COMPILER_GCC) || defined(FC_COMPILER_CLANG)
    __asm__ volatile (
        "cpuid"
        : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
        : "a"(eax), "c"(ecx)
    );
#else
    regs[0] = regs[1] = regs[2] = regs[3] = 0;
#endif
}

/**
 * @brief Check if a specific feature bit is set
 */
static inline int fc_has_bit(uint32_t reg, int bit) {
    return (reg & (1U << bit)) != 0;
}

#elif defined(FC_ARCH_ARM) || defined(FC_ARCH_ARM64)

/* ARM platform: detect NEON support */
#include <sys/auxv.h>

#ifndef HWCAP_NEON
#define HWCAP_NEON (1 << 12)
#endif

#ifndef HWCAP_NEON2
#define HWCAP_NEON2 (1 << 6)
#endif

#ifndef HWCAP2_NEON
#define HWCAP2_NEON (1 << 1)
#endif

static inline int fc_detect_arm_neon(void) {
    unsigned long hwcap = getauxval(AT_HWCAP);
    unsigned long hwcap2 = getauxval(AT_HWCAP2);

    if ((hwcap & (HWCAP_NEON | HWCAP_NEON2)) ||
        (hwcap2 & HWCAP2_NEON)) {
        return 1;
    }
    return 0;
}

#endif /* FC_ARCH_* */

/* ============================================================================
 * x86/x86_64 SIMD detection
 * ============================================================================ */

#if defined(FC_ARCH_X86) || defined(FC_ARCH_X86_64)

static fc_simd_level_t fc_detect_simd_x86(void) {
    uint32_t regs[4] = {0};
    uint32_t max_leaf = 0;
    uint32_t features_ecx = 0;
    uint32_t features_edx = 0;
    uint32_t extended_features_ebx = 0;

    /* Get maximum supported leaf */
    fc_cpuid(0, 0, regs);
    max_leaf = regs[0];

    /* Get basic features */
    if (max_leaf >= 1) {
        fc_cpuid(1, 0, regs);
        features_ecx = regs[2];
        features_edx = regs[3];
    }

    /* Get extended features for higher levels */
    if (max_leaf >= 7) {
        fc_cpuid(7, 0, regs);
        extended_features_ebx = regs[1];
    }

    /*
     * Feature bit definitions:
     * - SSE4.2: CPUID.(EAX=1):ECX.SSE42 [bit 20]
     * - AVX:    CPUID.(EAX=1):ECX.AVX [bit 28]
     * - AVX2:   CPUID.(EAX=7,ECX=0):EBX.AVX2 [bit 5]
     * - AVX-512F: CPUID.(EAX=7,ECX=0):EBX.AVX-512F [bit 16]
     * - AVX-512BW: CPUID.(EAX=7,ECX=0):EBX.AVX-512BW [bit 30]
     */

    /* Check SSE4.2 first (baseline requirement) */
    if (!fc_has_bit(features_ecx, 20)) {
        return FC_SIMD_SCALAR;
    }

    /* Check AVX */
    if (!fc_has_bit(features_ecx, 28)) {
        return FC_SIMD_SSE42;
    }

    /* Check AVX2 */
    if (!fc_has_bit(extended_features_ebx, 5)) {
        return FC_SIMD_AVX2;
    }

    /* Check AVX-512 */
    if (max_leaf >= 7) {
        fc_cpuid(7, 0, regs);
        extended_features_ebx = regs[1];

        if (fc_has_bit(extended_features_ebx, 16) &&
            fc_has_bit(extended_features_ebx, 30)) {
            return FC_SIMD_AVX512;
        }
    }

    return FC_SIMD_AVX2;
}

#elif defined(FC_ARCH_ARM) || defined(FC_ARCH_ARM64)

static fc_simd_level_t fc_detect_simd_arm(void) {
    if (fc_detect_arm_neon()) {
        return FC_SIMD_SSE42;
    }
    return FC_SIMD_SCALAR;
}

#else

static fc_simd_level_t fc_detect_simd_generic(void) {
    return FC_SIMD_SCALAR;
}

#endif /* FC_ARCH_* */

/* ============================================================================
 * Public API implementation
 * ============================================================================ */

fc_simd_level_t fc_detect_simd(void) {
#if defined(FC_ARCH_X86) || defined(FC_ARCH_X86_64)
    g_fc_simd_level = fc_detect_simd_x86();
#elif defined(FC_ARCH_ARM) || defined(FC_ARCH_ARM64)
    g_fc_simd_level = fc_detect_simd_arm();
#else
    g_fc_simd_level = fc_detect_simd_generic();
#endif
    return g_fc_simd_level;
}

fc_simd_level_t fc_get_simd_level(void) {
    return g_fc_simd_level;
}

const char* fc_simd_level_string(fc_simd_level_t level) {
    switch (level) {
        case FC_SIMD_SCALAR: return "Scalar (no SIMD)";
        case FC_SIMD_SSE42:  return "SSE4.2";
        case FC_SIMD_AVX2:   return "AVX2";
        case FC_SIMD_AVX512: return "AVX-512";
        default:              return "Unknown";
    }
}

size_t fc_simd_parallelism(fc_simd_level_t level) {
    switch (level) {
        case FC_SIMD_SCALAR: return 1;
        case FC_SIMD_SSE42:  return 2;
        case FC_SIMD_AVX2:   return 4;
        case FC_SIMD_AVX512: return 8;
        default:              return 1;
    }
}

/* Private: detection without global side effect (safe for Go runtime init) */
fc_simd_level_t fc_simd_detect_unsafe(void) {
#if defined(FC_ARCH_X86) || defined(FC_ARCH_X86_64)
    return fc_detect_simd_x86();
#elif defined(FC_ARCH_ARM) || defined(FC_ARCH_ARM64)
    return fc_detect_simd_arm();
#else
    return FC_SIMD_SCALAR;
#endif
}
