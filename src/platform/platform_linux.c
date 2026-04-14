/**
 * @file platform_linux.c
 * @brief Linux platform-specific implementation
 *
 * Provides Linux platform cache size detection and other system information.
 */

#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* ============================================================================
 * Cache size retrieval
 * ============================================================================ */

/* Read a single numeric value (with K/M/G suffix) from a sysfs file */
static size_t fc_read_cache_size(const char* path) {
    FILE* fp = fopen(path, "r");
    if (fp == NULL) return 0;

    size_t value = 0;
    char suffix = '\0';
    if (fscanf(fp, "%zu%c", &value, &suffix) >= 1) {
        switch (suffix) {
            case 'K': case 'k': value *= 1024; break;
            case 'M': case 'm': value *= 1024 * 1024; break;
            case 'G': case 'g': value *= 1024 * 1024 * 1024; break;
        }
    }
    fclose(fp);
    return value;
}

/* Fallback defaults when sysfs entries are unavailable */
#define FC_CACHE_L1_DEFAULT (32  * 1024ULL)
#define FC_CACHE_L2_DEFAULT (256 * 1024ULL)
#define FC_CACHE_L3_DEFAULT (8   * 1024 * 1024ULL)
#define FC_CACHE_LINE_DEFAULT 64

/* Get cache size for a specific level, with fallback to lower levels */
static size_t fc_get_cache_level(int level) {
    static const char* const paths[][4] = {
        [1] = { "/sys/devices/system/cpu/cpu0/cache/index1/size", NULL, NULL, NULL },
        [2] = { "/sys/devices/system/cpu/cpu0/cache/index2/size",
                "/sys/devices/system/cpu/cpu0/cache/index1/size", NULL, NULL },
        [3] = { "/sys/devices/system/cpu/cpu0/cache/index3/size",
                "/sys/devices/system/cpu/cpu0/cache/index2/size", NULL, NULL },
    };

    if (level < 1 || level > 3) return 0;

    for (int i = 0; i < 4 && paths[level][i]; i++) {
        size_t size = fc_read_cache_size(paths[level][i]);
        if (size > 0) return size;
    }
    return 0;
}

size_t fc_get_cache_line_size(void) {
#ifdef _SC_L1D_CACHE_LINESIZE
    long val = sysconf(_SC_L1D_CACHE_LINESIZE);
#else
    long val = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
#endif
    if (val > 0) return (size_t)val;

    size_t size = fc_read_cache_size(
        "/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size");
    return size > 0 ? size : FC_CACHE_LINE_DEFAULT;
}

size_t fc_get_l1_cache_size(void) {
    size_t size = fc_get_cache_level(1);
    return size > 0 ? size : FC_CACHE_L1_DEFAULT;
}

size_t fc_get_l2_cache_size(void) {
    size_t size = fc_get_cache_level(2);
    return size > 0 ? size : FC_CACHE_L2_DEFAULT;
}

size_t fc_get_l3_cache_size(void) {
    size_t size = fc_get_cache_level(3);
    return size > 0 ? size : FC_CACHE_L3_DEFAULT;
}
