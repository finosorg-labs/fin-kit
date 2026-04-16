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
#include <stddef.h>

/* Global SIMD level variable (read-only after initialization, thread-safe) */
fc_simd_level_t g_fc_simd_level = FC_SIMD_SCALAR;

/*
 * Platform-specific CPUID detection implementation
*/

#if defined(FC_ARCH_X86) || defined(FC_ARCH_X86_64)

/* x86/x86_64 platform: using inline assembly or intrinsics */

/**
 * @brief Execute CPUID instruction
 */
static inline void fc_cpuid(uint32_t eax, uint32_t ecx, uint32_t regs[4]) {
#if FC_COMPILER_GCC || FC_COMPILER_CLANG
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
 * @brief Execute XGETBV instruction
 */
static inline uint64_t fc_xgetbv(uint32_t ecx) {
#if FC_COMPILER_GCC || FC_COMPILER_CLANG
    uint32_t eax, edx;
    __asm__ volatile (
        "xgetbv"
        : "=a"(eax), "=d"(edx)
        : "c"(ecx)
    );
    return ((uint64_t)edx << 32) | eax;
#else
    return 0;
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
#if FC_OS_LINUX
#include <sys/auxv.h>
#include <asm/hwcap.h>

static inline int fc_detect_arm_neon(void) {
    unsigned long hwcap = getauxval(AT_HWCAP);
#ifdef HWCAP_ASIMD
    if (hwcap & HWCAP_ASIMD) return 1;
#endif
#ifdef HWCAP_NEON
    if (hwcap & HWCAP_NEON) return 1;
#endif
    return 0;
}
#elif FC_OS_MACOS
static inline int fc_detect_arm_neon(void) {
    return 1; /* Always on ARM64 macOS */
}
#else
static inline int fc_detect_arm_neon(void) {
    return 0;
}
#endif

#endif /* FC_ARCH_* */

/*
 * x86/x86_64 SIMD detection
*/

#if defined(FC_ARCH_X86) || defined(FC_ARCH_X86_64)

static fc_simd_level_t fc_detect_simd_x86(void) {
    uint32_t regs[4] = {0};
    uint32_t max_leaf = 0;
    uint32_t features_ecx = 0;
    uint32_t extended_features_ebx = 0;

    /* Get maximum supported leaf */
    fc_cpuid(0, 0, regs);
    max_leaf = regs[0];

    /* Get basic features (CPUID leaf 1) */
    if (max_leaf >= 1) {
        fc_cpuid(1, 0, regs);
        features_ecx = regs[2];
    }

    /* Check SSE4.2 first (bit 20 of ECX from leaf 1) */
    if (!fc_has_bit(features_ecx, 20)) {
        return FC_SIMD_SCALAR;
    }

    /* Check OSXSAVE (bit 27): OS must support XSAVE for AVX+ */
    if (!fc_has_bit(features_ecx, 27)) {
        return FC_SIMD_SSE42;
    }

    /* Check AVX (bit 28) and verify OS has enabled XMM/YMM state via XGETBV */
    if (!fc_has_bit(features_ecx, 28)) {
        return FC_SIMD_SSE42;
    }
    {
        uint64_t xcr0 = fc_xgetbv(0);
        if ((xcr0 & 0x6) != 0x6) { /* bit1=XMM, bit2=YMM */
            return FC_SIMD_SSE42;
        }
    }

    /* Get extended features (CPUID leaf 7, sub-leaf 0) for AVX2/AVX-512 */
    if (max_leaf >= 7) {
        fc_cpuid(7, 0, regs);
        extended_features_ebx = regs[1];
    }

    /* Check AVX2 (bit 5 of EBX from leaf 7) */
    if (!fc_has_bit(extended_features_ebx, 5)) {
        return FC_SIMD_SSE42; /* AVX supported but not AVX2 */
    }

    /* AVX2 is supported; check for AVX-512 */
    if (fc_has_bit(extended_features_ebx, 16) && fc_has_bit(extended_features_ebx, 30)) {
        /* AVX-512F (bit 16) and AVX-512BW (bit 30) both present */
        uint64_t xcr0 = fc_xgetbv(0);
        if ((xcr0 & 0xE0) == 0xE0) { /* bit5=OPMASK, bit6=ZMM_Hi256, bit7=Hi16_ZMM */
            return FC_SIMD_AVX512;
        }
    }

    return FC_SIMD_AVX2;
}

#elif defined(FC_ARCH_ARM) || defined(FC_ARCH_ARM64)

static fc_simd_level_t fc_detect_simd_arm(void) {
    if (fc_detect_arm_neon()) {
        return FC_SIMD_NEON;
    }
    return FC_SIMD_SCALAR;
}

#else

static fc_simd_level_t fc_detect_simd_generic(void) {
    return FC_SIMD_SCALAR;
}

#endif /* FC_ARCH_* */

/*
 * Public API implementation
*/

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
        case FC_SIMD_NEON:   return "ARM NEON";
        default:              return "Unknown";
    }
}

size_t fc_simd_parallelism(fc_simd_level_t level) {
    switch (level) {
        case FC_SIMD_SCALAR: return 1;
        case FC_SIMD_SSE42:  return 2;
        case FC_SIMD_AVX2:   return 4;
        case FC_SIMD_AVX512: return 8;
        case FC_SIMD_NEON:   return 2;
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
