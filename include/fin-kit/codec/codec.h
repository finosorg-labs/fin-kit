/**
 * @file codec.h
 * @brief Codec module public API
 *
 * This header provides the main entry point for the codec module,
 * which includes various encoding/decoding implementations.
 */

#ifndef FC_CODEC_H
#define FC_CODEC_H

#include <fin-kit/codec/fix_codec.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize FIX codec subsystem
 *
 * This function initializes the FIX protocol codec implementation.
 * It is called automatically by fc_init() and should not be called directly.
 */
void fc_fix_codec_init(void);

#ifdef __cplusplus
}
#endif

#endif /* FC_CODEC_H */
