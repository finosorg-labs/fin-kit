/**
 * @file platform_macos.c
 * @brief macOS platform-specific implementation
 *
 * Provides macOS platform cache size detection and other system information.
 */

#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/sysctl.h>

/* ============================================================================
 * Cache size retrieval using sysctl
 * ============================================================================ */

static size_t fc_sysctl_cache_size(const int* name, size_t default_size) {
    size_t size = default_size;
    size_t len = sizeof(size);

    if (sysctl((int*)name, 2, &size, &len, NULL, 0) != 0) {
        return default_size;
    }

    return size;
}

static size_t fc_sysctl_cache_line_size(const int* name, size_t default_size) {
    size_t line_size = default_size;
    size_t len = sizeof(line_size);

    if (sysctl((int*)name, 2, &line_size, &len, NULL, 0) != 0) {
        int cache_size_name[2] = {name[0], name[1]};
        size_t cache_size = default_size * 2;
        size_t cache_len = sizeof(cache_size);

        if (sysctl(cache_size_name, 2, &cache_size, &cache_len, NULL, 0) == 0) {
            return cache_size / 64;
        }
        return default_size;
    }

    return line_size;
}

size_t fc_get_cache_line_size(void) {
#if defined(FC_ARCH_ARM64)
    size_t line_size = 0;
    size_t len = sizeof(line_size);
    if (sysctlbyname("hw.cachelinesize", &line_size, &len, NULL, 0) == 0) {
        return line_size > 0 ? line_size : 64;
    }
    return 64;
#else
    int name[2] = {CTL_HW, HW_CACHELINE};
    return fc_sysctl_cache_line_size(name, 64);
#endif
}

size_t fc_get_l1_cache_size(void) {
#if defined(FC_ARCH_ARM64)
    size_t size = 0;
    size_t len = sizeof(size);
    if (sysctlbyname("hw.l1dcachesize", &size, &len, NULL, 0) == 0) {
        return size > 0 ? size : 128 * 1024;
    }
    return 128 * 1024;
#else
    int name[2] = {CTL_HW, HW_L1DCACHESIZE};
    return fc_sysctl_cache_size(name, 32 * 1024);
#endif
}

size_t fc_get_l2_cache_size(void) {
#if defined(FC_ARCH_ARM64)
    size_t size = 0;
    size_t len = sizeof(size);
    if (sysctlbyname("hw.l2cachesize", &size, &len, NULL, 0) == 0) {
        return size > 0 ? size : 4 * 1024 * 1024;
    }
    return 4 * 1024 * 1024;
#else
    int name[2] = {CTL_HW, HW_L2CACHESIZE};
    return fc_sysctl_cache_size(name, 256 * 1024);
#endif
}

size_t fc_get_l3_cache_size(void) {
#if defined(FC_ARCH_ARM64)
    size_t size = 0;
    size_t len = sizeof(size);
    if (sysctlbyname("hw.l3cachesize", &size, &len, NULL, 0) == 0) {
        return size > 0 ? size : 16 * 1024 * 1024;
    }
    return 16 * 1024 * 1024;
#else
    int name[2] = {CTL_HW, HW_L3CACHESIZE};
    return fc_sysctl_cache_size(name, 8 * 1024 * 1024);
#endif
}
