/**
 * @file platform_win.c
 * @brief Windows platform-specific implementation
 *
 * Provides Windows platform cache size detection and other system information.
 */

#include "platform.h"

#if defined(FC_OS_WINDOWS)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Fallback defaults */
#define FC_CACHE_LINE_DEFAULT 64
#define FC_CACHE_L1_DEFAULT   (32  * 1024ULL)
#define FC_CACHE_L2_DEFAULT   (256 * 1024ULL)
#define FC_CACHE_L3_DEFAULT   (8   * 1024 * 1024ULL)

/*
 * Cache size retrieval
*/

static size_t fc_get_cache_size_win(DWORD level) {
    DWORD bufferSize = 0;
    if (GetLogicalProcessorInformation(NULL, &bufferSize) == FALSE &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return 0;
    }

    SYSTEM_LOGICAL_PROCESSOR_INFORMATION* buffer =
        (SYSTEM_LOGICAL_PROCESSOR_INFORMATION*)malloc(bufferSize);
    if (buffer == NULL) return 0;

    if (GetLogicalProcessorInformation(buffer, &bufferSize) == FALSE) {
        free(buffer);
        return 0;
    }

    DWORD count = bufferSize / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
    size_t total = 0;

    for (DWORD i = 0; i < count; i++) {
        if (buffer[i].Relationship == RelationCache &&
            buffer[i].Cache.Level == level &&
            buffer[i].Cache.Type == CacheData) {
            total += buffer[i].Cache.Size;
        }
    }

    free(buffer);
    return total;
}

size_t fc_get_cache_line_size(void) {
    DWORD bufferSize = 0;
    if (GetLogicalProcessorInformation(NULL, &bufferSize) == FALSE &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return FC_CACHE_LINE_DEFAULT;
    }

    SYSTEM_LOGICAL_PROCESSOR_INFORMATION* buffer =
        (SYSTEM_LOGICAL_PROCESSOR_INFORMATION*)malloc(bufferSize);
    if (buffer == NULL) return FC_CACHE_LINE_DEFAULT;

    if (GetLogicalProcessorInformation(buffer, &bufferSize) == FALSE) {
        free(buffer);
        return FC_CACHE_LINE_DEFAULT;
    }

    DWORD count = bufferSize / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
    for (DWORD i = 0; i < count; i++) {
        if (buffer[i].Relationship == RelationCache) {
            size_t lineSize = buffer[i].Cache.LineSize;
            free(buffer);
            return lineSize > 0 ? lineSize : FC_CACHE_LINE_DEFAULT;
        }
    }

    free(buffer);
    return FC_CACHE_LINE_DEFAULT;
}

size_t fc_get_l1_cache_size(void) {
    size_t size = fc_get_cache_size_win(1);
    return size > 0 ? size : FC_CACHE_L1_DEFAULT;
}

size_t fc_get_l2_cache_size(void) {
    size_t size = fc_get_cache_size_win(2);
    return size > 0 ? size : FC_CACHE_L2_DEFAULT;
}

size_t fc_get_l3_cache_size(void) {
    size_t size = fc_get_cache_size_win(3);
    return size > 0 ? size : FC_CACHE_L3_DEFAULT;
}

#else

/* Non-Windows fallback (should not be linked, but provides compilation safety) */
size_t fc_get_cache_line_size(void) { return FC_CACHE_LINE_DEFAULT; }
size_t fc_get_l1_cache_size(void)    { return FC_CACHE_L1_DEFAULT; }
size_t fc_get_l2_cache_size(void)    { return FC_CACHE_L2_DEFAULT; }
size_t fc_get_l3_cache_size(void)    { return FC_CACHE_L3_DEFAULT; }

#endif /* FC_OS_WINDOWS */
