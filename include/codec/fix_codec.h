/**
 * @file fix_codec.h
 * @brief FIX protocol high-performance encoding and decoding
 *
 * This module provides low-level byte processing capabilities for FIX messages,
 * designed to be called from Go via cgo.
 *
 * Core features:
 *   1. Message boundary detection + field splitting + tag classification (Header/Body/Trailer)
 *   2. CheckSum and BodyLength pre-calculation (completed in a single parse pass)
 *   3. Message serialization (assembling complete FIX byte stream from structured fields)
 *
 * Design principles:
 *   - All offsets are relative to message start position, enabling zero-copy slicing on Go side
 *   - Batch processing: single cgo call can parse multiple messages, amortizing call overhead
 *   - SIMD acceleration: checksum calculation uses AVX2/SSE4.2/Scalar runtime dispatch
 */

#ifndef FC_FIX_CODEC_H
#define FC_FIX_CODEC_H

#include <platform.h>
#include <error.h>

FC_BEGIN_DECLS

/**
 * @brief FIX protocol constants
 */
#define FC_FIX_MAX_FIELDS_PER_MSG  256   /**< Maximum fields per message */
#define FC_FIX_MAX_MESSAGES        16    /**< Maximum messages per batch parse */
#define FC_FIX_SOH                 0x01  /**< FIX field separator (Start Of Header) */
#define FC_FIX_SINGLE_MAX_FIELDS   128   /**< Maximum fields for single message parse */

/**
 * @brief Field section classification
 */
typedef enum {
    FC_FIX_SECTION_HEADER  = 0,  /**< Message header */
    FC_FIX_SECTION_BODY    = 1,  /**< Message body */
    FC_FIX_SECTION_TRAILER = 2,  /**< Message trailer */
} fc_fix_section_t;

/**
 * @brief Required header tag bitmap (for quick validation)
 *
 * Each bit corresponds to a required FIX protocol standard header tag
 */
#define FC_FIX_REQBIT_BEGINSTRING  (1u << 0)  /**< tag 8  - BeginString */
#define FC_FIX_REQBIT_BODYLENGTH   (1u << 1)  /**< tag 9  - BodyLength */
#define FC_FIX_REQBIT_MSGTYPE      (1u << 2)  /**< tag 35 - MsgType */
#define FC_FIX_REQBIT_MSGSEQNUM    (1u << 3)  /**< tag 34 - MsgSeqNum */
#define FC_FIX_REQBIT_SENDERCOMPID (1u << 4)  /**< tag 49 - SenderCompID */
#define FC_FIX_REQBIT_TARGETCOMPID (1u << 5)  /**< tag 56 - TargetCompID */
#define FC_FIX_REQBIT_SENDINGTIME  (1u << 6)  /**< tag 52 - SendingTime */
#define FC_FIX_REQBIT_CHECKSUM     (1u << 7)  /**< tag 10 - CheckSum */
#define FC_FIX_REQBIT_ALL_BASIC    0xFFu      /**< All basic required tags */

/**
 * @brief Parsed field information
 *
 * All offsets (value_offset, raw_offset) are relative to the message start
 * position (fc_fix_parsed_message_t.msg_offset), enabling zero-copy slicing
 * on the Go side.
 */
typedef struct {
    int32_t  tag;             /**< Tag number (e.g., 8, 35, 49) */
    uint32_t value_offset;    /**< Value start offset (after '=') */
    uint16_t value_length;    /**< Value byte length (excluding SOH) */
    uint32_t raw_offset;      /**< Raw field start offset ("tag=value\x01") */
    uint16_t raw_length;      /**< Raw field byte length (including SOH) */
    uint8_t  section;         /**< Section: FC_FIX_SECTION_HEADER/BODY/TRAILER */
    uint8_t  _pad;            /**< Alignment padding */
} fc_fix_parsed_field_t;

/**
 * @brief Parsed message information
 *
 * Contains message position, length, all fields, and pre-calculated CheckSum
 * and BodyLength. Go side can directly use computed_* vs declared_* for
 * zero-overhead validation without re-traversing fields.
 */
typedef struct {
    uint32_t              msg_offset;           /**< Message start offset in input buffer */
    uint32_t              msg_length;           /**< Total message byte count (including CheckSum SOH) */
    uint32_t              computed_checksum;    /**< C-computed raw byte sum (Go side %256 for comparison) */
    uint32_t              computed_body_length; /**< C-computed BodyLength */
    uint32_t              declared_checksum;    /**< Declared CheckSum value from tag 10 */
    uint32_t              declared_body_length; /**< Declared BodyLength value from tag 9 */
    int32_t               field_count;          /**< Total field count */
    int32_t               header_field_count;   /**< Header field count */
    int32_t               body_field_count;     /**< Body field count */
    int32_t               trailer_field_count;  /**< Trailer field count */
    uint32_t              required_tags_bitmap; /**< Required tag presence bitmap */
    fc_fix_parsed_field_t fields[FC_FIX_MAX_FIELDS_PER_MSG]; /**< All fields array */
} fc_fix_parsed_message_t;

/**
 * @brief Batch parse result
 *
 * A single fc_fix_parse_messages call can return multiple complete message
 * parse results. consumed_bytes indicates how many bytes in the input buffer
 * have been successfully consumed, allowing Go side to advance read pointer.
 */
typedef struct {
    int32_t                  msg_count;       /**< Number of successfully parsed messages */
    int32_t                  consumed_bytes;  /**< Total bytes consumed from input buffer */
    fc_fix_parsed_message_t  messages[FC_FIX_MAX_MESSAGES]; /**< Parse results for each message */
} fc_fix_parse_result_t;

/**
 * @brief Single message parse result (lightweight version)
 *
 * Difference from fc_fix_parse_result_t:
 *   - Supports only 1 message (not 16), structure size ~2.3KB (not ~74KB)
 *   - Field limit 128 (not 256), covers 99.9% of FIX messages
 *   - Suitable for Unmarshal and other single-message parse scenarios,
 *     avoiding large structure allocation overhead
 */
typedef struct {
    int32_t               success;              /**< Non-zero if complete message found and parsed */
    uint32_t              msg_offset;           /**< Message start offset in input buffer */
    uint32_t              msg_length;           /**< Total message byte count */
    uint32_t              computed_checksum;    /**< C-computed raw byte sum */
    uint32_t              computed_body_length; /**< C-computed BodyLength */
    uint32_t              declared_checksum;    /**< Declared CheckSum value from tag 10 */
    uint32_t              declared_body_length; /**< Declared BodyLength value from tag 9 */
    int32_t               field_count;          /**< Total field count */
    int32_t               header_field_count;   /**< Header field count */
    int32_t               body_field_count;     /**< Body field count */
    int32_t               trailer_field_count;  /**< Trailer field count */
    uint32_t              required_tags_bitmap; /**< Required tag presence bitmap */
    fc_fix_parsed_field_t fields[FC_FIX_SINGLE_MAX_FIELDS]; /**< All fields array */
} fc_fix_parse_single_result_t;

/**
 * @brief Field for marshaling
 *
 * Go side packs sorted fields (excluding tags 8/9/10) into this structure
 * and passes to C. The value pointer points to Go memory, guaranteed valid
 * during cgo call by runtime.
 */
typedef struct {
    int32_t        tag;           /**< Tag number */
    const uint8_t* value;         /**< Value byte pointer */
    uint16_t       value_length;  /**< Value byte length */
} fc_fix_marshal_field_t;

/**
 * @brief Marshal result
 */
typedef struct {
    int32_t  success;        /**< Non-zero indicates success */
    uint32_t output_length;  /**< Actual bytes written to output buffer */
    uint32_t body_length;    /**< Calculated BodyLength */
    uint32_t checksum;       /**< Calculated CheckSum (already %256) */
} fc_fix_marshal_result_t;

/**
 * @brief Parse FIX messages from raw byte stream (batch mode)
 *
 * Automatically detects message boundaries (finds "8=..." start and "10=..." end),
 * and completes field splitting + tag classification + CheckSum/BodyLength
 * calculation in a single pass for each message.
 *
 * Incomplete messages are not consumed (consumed_bytes excludes them).
 *
 * @param[in]  data        Input byte buffer (must be contiguous)
 * @param[in]  data_length Buffer length
 * @param[out] result      Output structure, allocated by caller
 *
 * @return Number of successfully parsed messages
 *
 * @note Thread-safe after library initialization
 * @note Time complexity: O(n) where n is data_length
 * @note Space complexity: O(1) - no heap allocation
 */
int fc_fix_parse_messages(
    const uint8_t*         data,
    size_t                 data_length,
    fc_fix_parse_result_t* result
);

/**
 * @brief Parse single FIX message (lightweight version)
 *
 * Same functionality as fc_fix_parse_messages, but only parses the first
 * complete message, using fc_fix_parse_single_result_t (~2.3KB) instead of
 * fc_fix_parse_result_t (~74KB), significantly reducing Go-side memory
 * allocation overhead.
 *
 * Suitable for Unmarshal and other scenarios with known single message.
 *
 * @param[in]  data        Input byte buffer
 * @param[in]  data_length Buffer length
 * @param[out] result      Output structure, allocated by caller
 *
 * @return 1 if message found and parsed, 0 otherwise
 *
 * @note Thread-safe after library initialization
 * @note Time complexity: O(n) where n is data_length
 * @note Space complexity: O(1) - no heap allocation
 */
int fc_fix_parse_one(
    const uint8_t*                data,
    size_t                        data_length,
    fc_fix_parse_single_result_t* result
);

/**
 * @brief Zero-allocation single message parse (SoA layout)
 *
 * Writes parse results directly into caller-provided flat arrays, avoiding
 * any C-side structure allocation. Go side pre-allocates int32_t[] / uint32_t[] /
 * uint16_t[] / uint8_t[] arrays and passes pointers; C layer only fills them.
 *
 * @param[in]  data                     Raw message bytes
 * @param[in]  data_length              Message length
 * @param[out] out_tags                 Each field's tag number
 * @param[out] out_value_offs           Each field's value offset (relative to message start)
 * @param[out] out_value_lens           Each field's value length
 * @param[out] out_raw_offs             Each field's raw field offset (relative to message start)
 * @param[out] out_raw_lens             Each field's raw field length (including SOH)
 * @param[out] out_sections             Each field's section classification
 * @param[in]  max_fields               Output array capacity
 * @param[out] out_msg_offset           Message start offset in data
 * @param[out] out_msg_length           Message total length
 * @param[out] out_checksum             Computed checksum (raw byte sum, needs %256)
 * @param[out] out_body_length          Computed BodyLength
 * @param[out] out_declared_checksum    Declared checksum from tag 10
 * @param[out] out_declared_body_length Declared BodyLength from tag 9
 *
 * @return Field count, 0 if no complete message found
 *
 * @note Thread-safe after library initialization
 * @note Time complexity: O(n) where n is data_length
 * @note Space complexity: O(1) - no heap allocation
 */
int fc_fix_parse_fields_into(
    const uint8_t* data,
    size_t         data_length,
    int32_t*       out_tags,
    uint32_t*      out_value_offs,
    uint16_t*      out_value_lens,
    uint32_t*      out_raw_offs,
    uint16_t*      out_raw_lens,
    uint8_t*       out_sections,
    int            max_fields,
    uint32_t*      out_msg_offset,
    uint32_t*      out_msg_length,
    uint32_t*      out_checksum,
    uint32_t*      out_body_length,
    uint32_t*      out_declared_checksum,
    uint32_t*      out_declared_body_length
);

/**
 * @brief Zero-allocation single message parse (AoS layout)
 *
 * Same functionality as fc_fix_parse_fields_into, but outputs to a
 * fc_fix_parsed_field_t[] structure array (instead of 6 separate flat arrays).
 * Each field's attributes are stored contiguously in memory, providing better
 * cache locality during Go-side traversal and reducing indirect addressing overhead.
 *
 * @param[in]  data                     Raw message bytes
 * @param[in]  data_length              Message length
 * @param[out] out_fields               Output field array
 * @param[in]  max_fields               Output array capacity
 * @param[out] out_msg_offset           Message start offset
 * @param[out] out_msg_length           Message total length
 * @param[out] out_checksum             Computed checksum
 * @param[out] out_body_length          Computed BodyLength
 * @param[out] out_declared_checksum    Declared checksum
 * @param[out] out_declared_body_length Declared BodyLength
 *
 * @return Field count, 0 if no complete message found
 *
 * @note Thread-safe after library initialization
 * @note Time complexity: O(n) where n is data_length
 * @note Space complexity: O(1) - no heap allocation
 */
int fc_fix_parse_fields_aos(
    const uint8_t*         data,
    size_t                 data_length,
    fc_fix_parsed_field_t* out_fields,
    int                    max_fields,
    uint32_t*              out_msg_offset,
    uint32_t*              out_msg_length,
    uint32_t*              out_checksum,
    uint32_t*              out_body_length,
    uint32_t*              out_declared_checksum,
    uint32_t*              out_declared_body_length
);

/**
 * @brief Assemble structured fields into complete FIX message byte stream
 *
 * Automatically calculates and fills in BodyLength (tag 9) and CheckSum (tag 10).
 * Output format: 8=xxx\x01 9=nnn\x01 [fields...] 10=ccc\x01
 *
 * @param[in]  begin_string     BeginString value (e.g., "FIX.4.4")
 * @param[in]  begin_string_len BeginString value length
 * @param[in]  fields           Sorted field array (excluding tags 8, 9, 10)
 * @param[in]  field_count      Field count
 * @param[out] output_buf       Output buffer, pre-allocated by caller
 * @param[in]  output_buf_cap   Output buffer capacity
 *
 * @return fc_fix_marshal_result_t structure
 *
 * @note Thread-safe after library initialization
 * @note Time complexity: O(n) where n is total field bytes
 * @note Space complexity: O(1) - no heap allocation
 */
fc_fix_marshal_result_t fc_fix_marshal_message(
    const uint8_t*               begin_string,
    int32_t                      begin_string_len,
    const fc_fix_marshal_field_t* fields,
    int32_t                      field_count,
    uint8_t*                     output_buf,
    size_t                       output_buf_cap
);

/**
 * @brief Calculate byte sum (for FIX CheckSum)
 *
 * Returns arithmetic sum of all bytes (raw value, not modulo).
 * Caller needs to %256 to get final 3-digit CheckSum.
 *
 * Uses SIMD acceleration (AVX2/SSE4.2/Scalar runtime dispatch).
 *
 * @param[in] data   Byte buffer
 * @param[in] length Buffer length
 *
 * @return Raw byte sum
 *
 * @note Thread-safe after library initialization
 * @note Time complexity: O(n) where n is length
 * @note Space complexity: O(1)
 */
uint32_t fc_fix_checksum(const uint8_t* data, size_t length);

/**
 * @brief Get current SIMD acceleration level
 *
 * Returns static string: "avx2", "sse4.2", or "scalar".
 * For diagnostics and logging. Must be called after fc_fix_parse_messages /
 * fc_fix_checksum (they trigger initialization).
 *
 * @return SIMD level string
 *
 * @note Thread-safe after library initialization
 */
const char* fc_fix_simd_level(void);

FC_END_DECLS

#endif /* FC_FIX_CODEC_H */
