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

/* ============================================================================
 * Cache size retrieval
 * ============================================================================ */

static size_t fc_get_cache_size_win(DWORD level) {
    DWORD bufferSize = 0;
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION* buffer = NULL;

    BOOL result = GetLogicalProcessorInformation(NULL, &bufferSize);
    if (result != FALSE || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return 0;
    }

    buffer = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION*)malloc(bufferSize);
    if (buffer == NULL) {
        return 0;
    }

    result = GetLogicalProcessorInformation(buffer, &bufferSize);
    if (result == FALSE) {
        free(buffer);
        return 0;
    }

    DWORD count = bufferSize / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
    size_t totalCacheSize = 0;

    for (DWORD i = 0; i < count; i++) {
        if (buffer[i].Relationship == RelationCache &&
            buffer[i].Cache.Level == level &&
            buffer[i].Cache.Type == CacheData) {
            totalCacheSize += buffer[i].Cache.Size;
        }
    }

    free(buffer);
    return totalCacheSize;
}

size_t fc_get_cache_line_size(void) {
    DWORD bufferSize = 0;
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION* buffer = NULL;

    BOOL result = GetLogicalProcessorInformation(NULL, &bufferSize);
    if (result != FALSE || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return 64;
    }

    buffer = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION*)malloc(bufferSize);
    if (buffer == NULL) {
        return 64;
    }

    result = GetLogicalProcessorInformation(buffer, &bufferSize);
    if (result == FALSE) {
        free(buffer);
        return 64;
    }

    DWORD count = bufferSize / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
    for (DWORD i = 0; i < count; i++) {
        if (buffer[i].Relationship == RelationCache) {
            size_t lineSize = buffer[i].Cache.LineSize;
            free(buffer);
            return lineSize > 0 ? lineSize : 64;
        }
    }

    free(buffer);
    return 64;
}

size_t fc_get_l1_cache_size(void) {
    size_t size = fc_get_cache_size_win(1);
    return size > 0 ? size : 32 * 1024;
}

size_t fc_get_l2_cache_size(void) {
    size_t size = fc_get_cache_size_win(2);
    return size > 0 ? size : 256 * 1024;
}

size_t fc_get_l3_cache_size(void) {
    size_t size = fc_get_cache_size_win(3);
    return size > 0 ? size : 8 * 1024 * 1024;
}

#else

/* Non-Windows platform: provide fallback implementations */

size_t fc_get_cache_line_size(void) { return 64; }
size_t fc_get_l1_cache_size(void)    { return 32 * 1024; }
size_t fc_get_l2_cache_size(void)    { return 256 * 1024; }
size_t fc_get_l3_cache_size(void)    { return 8 * 1024 * 1024; }

#endif /* FC_OS_WINDOWS */
