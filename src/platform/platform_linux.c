/**
 * @file platform_linux.c
 * @brief Linux platform-specific implementation
 *
 * Provides Linux platform cache size detection and other system information.
 */

#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/sysinfo.h>
#include <sys/types.h>

/* ============================================================================
 * Cache size retrieval
 * ============================================================================ */

/* Read cache size from /sys/devices/system/cpu/cpu0/cache/ */
static size_t fc_read_cache_size(const char* path) {
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        return 0;
    }

    size_t size = 0;
    char line[64];

    while (fgets(line, sizeof(line), fp) != NULL) {
        char* ptr = line;
        while (*ptr == ' ' || *ptr == '\t') ptr++;

        size_t value = 0;
        while (*ptr >= '0' && *ptr <= '9') {
            value = value * 10 + (*ptr - '0');
            ptr++;
        }

        if (*ptr == 'K' || *ptr == 'k') {
            value *= 1024;
        } else if (*ptr == 'M' || *ptr == 'm') {
            value *= 1024 * 1024;
        } else if (*ptr == 'G' || *ptr == 'g') {
            value *= 1024 * 1024 * 1024;
        }

        size = value;
    }

    fclose(fp);
    return size;
}

/* Get cache size for a specific level */
static size_t fc_get_cache_level(int level) {
    char path[256];

    if (level == 1) {
        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu0/cache/index1/size");
        size_t size = fc_read_cache_size(path);
        if (size > 0) return size;
    } else if (level == 2) {
        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu0/cache/index2/size");
        size_t size = fc_read_cache_size(path);
        if (size > 0) return size;

        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu0/cache/index1/size");
        size = fc_read_cache_size(path);
        return size;
    } else if (level == 3) {
        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu0/cache/index3/size");
        size_t size = fc_read_cache_size(path);
        if (size > 0) return size;

        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu0/cache/index2/size");
        size = fc_read_cache_size(path);
        return size;
    }

    return 0;
}

size_t fc_get_cache_line_size(void) {
    long val = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
    if (val > 0) {
        return (size_t)val;
    }

    size_t size = fc_read_cache_size(
        "/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size");
    if (size > 0) {
        return size;
    }

    return 64;
}

size_t fc_get_l1_cache_size(void) {
    size_t size = fc_get_cache_level(1);
    return size > 0 ? size : 32 * 1024;
}

size_t fc_get_l2_cache_size(void) {
    size_t size = fc_get_cache_level(2);
    return size > 0 ? size : 256 * 1024;
}

size_t fc_get_l3_cache_size(void) {
    size_t size = fc_get_cache_level(3);
    return size > 0 ? size : 8 * 1024 * 1024;
}
