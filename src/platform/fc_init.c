/**
 * @file fc_init.c
 * @brief Library initialization and version query implementation
 */

#include <fin-kit/platform/platform.h>
#include <fin-kit/platform/simd_detect.h>
#include <fin-kit/platform/error.h>

#include <stdatomic.h>

/* Library initialization state: 0=not init, 1=initialized */
static atomic_int g_fc_initialized = 0;

int fc_init(void) {
    int expected = 0;
    if (!atomic_compare_exchange_strong(&g_fc_initialized, &expected, 1)) {
        /* Already initialized, that is fine */
        return FC_OK;
    }

    /* Detect SIMD capabilities */
    fc_detect_simd();

    return FC_OK;
}

void fc_cleanup(void) {
    atomic_store(&g_fc_initialized, 0);
}

int fc_version(void) {
    return (FC_VERSION_MAJOR << 16) | (FC_VERSION_MINOR << 8) | FC_VERSION_PATCH;
}

int fc_version_major(void) {
    return FC_VERSION_MAJOR;
}

int fc_version_minor(void) {
    return FC_VERSION_MINOR;
}

int fc_version_patch(void) {
    return FC_VERSION_PATCH;
}

const char* fc_version_string(void) {
    return FC_VERSION_STRING;
}
