/*
 * test_fix_codec.c - FIX Protocol Codec Tests
 */

#include "test_framework.h"
#include <codec/fix_codec.h>
#include <string.h>

/*
 * Checksum Tests
*/

/* Test FIX checksum - empty data */
TEST(test_fix_checksum_empty) {
    uint32_t checksum = fc_fix_checksum(NULL, 0);
    ASSERT_EQ(checksum, 0);
}

/* Test FIX checksum - NULL pointer */
TEST(test_fix_checksum_null) {
    uint32_t checksum = fc_fix_checksum(NULL, 100);
    ASSERT_EQ(checksum, 0);
}

/* Test FIX checksum calculation */
TEST(test_fix_checksum) {
    const char* data = "8=FIX.4.2\x01" "9=40\x01" "35=D\x01";
    uint32_t checksum = fc_fix_checksum((const uint8_t*)data, strlen(data));
    FC_TEST_ASSERT_NONZERO(checksum);
}

/* Test FIX checksum - various sizes */
TEST(test_fix_checksum_sizes) {
    const uint8_t data[100];
    memset((void*)data, 'A', sizeof(data));

    /* Test different sizes to exercise SIMD paths */
    uint32_t cs1 = fc_fix_checksum(data, 1);
    uint32_t cs10 = fc_fix_checksum(data, 10);
    uint32_t cs50 = fc_fix_checksum(data, 50);
    uint32_t cs100 = fc_fix_checksum(data, 100);

    FC_TEST_ASSERT_NONZERO(cs1);
    FC_TEST_ASSERT_NONZERO(cs10);
    FC_TEST_ASSERT_NONZERO(cs50);
    FC_TEST_ASSERT_NONZERO(cs100);
}

/*
 * Parse Messages (Batch) Tests
*/

/* Test FIX message parsing - NULL result */
TEST(test_fix_parse_null_result) {
    const char* msg = "8=FIX.4.4\x01" "9=40\x01" "35=D\x01";
    int status = fc_fix_parse_messages((const uint8_t*)msg, strlen(msg), NULL);
    ASSERT_EQ(status, FC_ERR_INVALID_ARG);
}

/* Test FIX message parsing - empty input */
TEST(test_fix_parse_empty) {
    fc_fix_parse_result_t result;
    int status = fc_fix_parse_messages((const uint8_t*)"", 0, &result);
    ASSERT_EQ(status, 0);
    ASSERT_EQ(result.msg_count, 0);
}

/* Test FIX message parsing - basic */
TEST(test_fix_parse_basic) {
    const char* msg = "8=FIX.4.4\x01" "9=40\x01" "35=D\x01"
                     "49=SENDER\x01" "56=TARGET\x01" "10=000\x01";

    fc_fix_parse_result_t result;
    int status = fc_fix_parse_messages((const uint8_t*)msg, strlen(msg), &result);

    ASSERT_EQ(status, 1);
    ASSERT_EQ(result.msg_count, 1);
    FC_TEST_ASSERT_NONZERO(result.consumed_bytes);
    FC_TEST_ASSERT_NONZERO(result.messages[0].field_count);
}

/* Test FIX message parsing - incomplete message */
TEST(test_fix_parse_incomplete) {
    const char* msg = "8=FIX.4.4\x01" "9=40\x01" "35=D\x01";  /* No checksum */

    fc_fix_parse_result_t result;
    int status = fc_fix_parse_messages((const uint8_t*)msg, strlen(msg), &result);

    ASSERT_EQ(status, 0);
    ASSERT_EQ(result.msg_count, 0);
}

/* Test FIX message parsing - multiple messages */
TEST(test_fix_parse_multiple) {
    const char* msg = "8=FIX.4.4\x01" "9=40\x01" "35=D\x01" "10=000\x01"
                     "8=FIX.4.4\x01" "9=40\x01" "35=A\x01" "10=000\x01";

    fc_fix_parse_result_t result;
    int status = fc_fix_parse_messages((const uint8_t*)msg, strlen(msg), &result);

    ASSERT_EQ(status, 2);
    ASSERT_EQ(result.msg_count, 2);
}

/* Test FIX message parsing - no start marker */
TEST(test_fix_parse_no_start) {
    const char* msg = "35=D\x01" "49=SENDER\x01" "10=000\x01";

    fc_fix_parse_result_t result;
    int status = fc_fix_parse_messages((const uint8_t*)msg, strlen(msg), &result);

    ASSERT_EQ(status, 0);
    ASSERT_EQ(result.msg_count, 0);
}

/* Test FIX message parsing - with body length */
TEST(test_fix_parse_with_body_length) {
    const char* msg = "8=FIX.4.4\x01" "9=50\x01" "35=D\x01"
                     "49=SENDER\x01" "56=TARGET\x01" "34=1\x01" "10=000\x01";

    fc_fix_parse_result_t result;
    int status = fc_fix_parse_messages((const uint8_t*)msg, strlen(msg), &result);

    ASSERT_EQ(status, 1);
    FC_TEST_ASSERT_NONZERO(result.messages[0].declared_body_length);
    FC_TEST_ASSERT_NONZERO(result.messages[0].computed_body_length);
}

/*
 * Parse One (Single Message) Tests
*/

/* Test fc_fix_parse_one - NULL result */
TEST(test_fix_parse_one_null) {
    const char* msg = "8=FIX.4.4\x01" "9=40\x01" "35=D\x01" "10=000\x01";
    int status = fc_fix_parse_one((const uint8_t*)msg, strlen(msg), NULL);
    ASSERT_EQ(status, 0);
}

/* Test fc_fix_parse_one - NULL data */
TEST(test_fix_parse_one_null_data) {
    fc_fix_parse_single_result_t result;
    int status = fc_fix_parse_one(NULL, 100, &result);
    ASSERT_EQ(status, 0);
}

/* Test fc_fix_parse_one - valid message */
TEST(test_fix_parse_one_valid) {
    const char* msg = "8=FIX.4.4\x01" "9=40\x01" "35=D\x01"
                     "49=SENDER\x01" "56=TARGET\x01" "10=000\x01";

    fc_fix_parse_single_result_t result;
    int status = fc_fix_parse_one((const uint8_t*)msg, strlen(msg), &result);

    ASSERT_EQ(status, 1);
    FC_TEST_ASSERT_NONZERO(result.success);
    FC_TEST_ASSERT_NONZERO(result.field_count);
    FC_TEST_ASSERT_NONZERO(result.msg_length);
}

/* Test fc_fix_parse_one - incomplete message */
TEST(test_fix_parse_one_incomplete) {
    const char* msg = "8=FIX.4.4\x01" "9=40\x01" "35=D\x01";

    fc_fix_parse_single_result_t result;
    int status = fc_fix_parse_one((const uint8_t*)msg, strlen(msg), &result);

    ASSERT_EQ(status, 0);
    ASSERT_EQ(result.success, 0);
}

/* Test fc_fix_parse_one - empty input */
TEST(test_fix_parse_one_empty) {
    fc_fix_parse_single_result_t result;
    int status = fc_fix_parse_one((const uint8_t*)"", 0, &result);

    ASSERT_EQ(status, 0);
    ASSERT_EQ(result.success, 0);
}

/* Test fc_fix_parse_one - many fields */
TEST(test_fix_parse_one_many_fields) {
    char msg[2048];
    int pos = 0;

    pos += snprintf(msg + pos, sizeof(msg) - pos, "8=FIX.4.4\x01" "9=500\x01");

    /* Add many fields */
    for (int i = 100; i < 120; i++) {
        pos += snprintf(msg + pos, sizeof(msg) - pos, "%d=VAL%d\x01", i, i);
        if (pos >= (int)sizeof(msg) - 50) break;  /* Safety check */
    }

    pos += snprintf(msg + pos, sizeof(msg) - pos, "10=000\x01");

    fc_fix_parse_single_result_t result;
    int status = fc_fix_parse_one((const uint8_t*)msg, pos, &result);

    ASSERT_EQ(status, 1);
    FC_TEST_ASSERT_NONZERO(result.success);
    FC_TEST_ASSERT_NONZERO(result.field_count);
}

/*
 * Parse Fields Into (SoA) Tests
*/

/* Test fc_fix_parse_fields_into - NULL checks */
TEST(test_fix_parse_fields_into_null) {
    const char* msg = "8=FIX.4.4\x01" "9=40\x01" "35=D\x01" "10=000\x01";

    int32_t tags[10];
    uint32_t value_offs[10], raw_offs[10];
    uint16_t value_lens[10], raw_lens[10];
    uint8_t sections[10];
    uint32_t msg_offset, msg_length, checksum, body_length;
    uint32_t declared_checksum, declared_body_length;

    /* NULL data */
    int status = fc_fix_parse_fields_into(
        NULL, strlen(msg), tags, value_offs, value_lens, raw_offs, raw_lens,
        sections, 10, &msg_offset, &msg_length, &checksum, &body_length,
        &declared_checksum, &declared_body_length);
    ASSERT_EQ(status, 0);

    /* NULL tags */
    status = fc_fix_parse_fields_into(
        (const uint8_t*)msg, strlen(msg), NULL, value_offs, value_lens, raw_offs, raw_lens,
        sections, 10, &msg_offset, &msg_length, &checksum, &body_length,
        &declared_checksum, &declared_body_length);
    ASSERT_EQ(status, 0);
}

/* Test fc_fix_parse_fields_into - valid message */
TEST(test_fix_parse_fields_into_valid) {
    const char* msg = "8=FIX.4.4\x01" "9=40\x01" "35=D\x01"
                     "49=SENDER\x01" "56=TARGET\x01" "10=000\x01";

    int32_t tags[20];
    uint32_t value_offs[20], raw_offs[20];
    uint16_t value_lens[20], raw_lens[20];
    uint8_t sections[20];
    uint32_t msg_offset, msg_length, checksum, body_length;
    uint32_t declared_checksum, declared_body_length;

    int field_count = fc_fix_parse_fields_into(
        (const uint8_t*)msg, strlen(msg), tags, value_offs, value_lens, raw_offs, raw_lens,
        sections, 20, &msg_offset, &msg_length, &checksum, &body_length,
        &declared_checksum, &declared_body_length);

    FC_TEST_ASSERT_NONZERO(field_count);
    FC_TEST_ASSERT_NONZERO(msg_length);
}

/* Test fc_fix_parse_fields_into - field limit */
TEST(test_fix_parse_fields_into_limit) {
    const char* msg = "8=FIX.4.4\x01" "9=40\x01" "35=D\x01"
                     "49=SENDER\x01" "56=TARGET\x01" "10=000\x01";

    int32_t tags[3];
    uint32_t value_offs[3], raw_offs[3];
    uint16_t value_lens[3], raw_lens[3];
    uint8_t sections[3];
    uint32_t msg_offset, msg_length, checksum, body_length;
    uint32_t declared_checksum, declared_body_length;

    int field_count = fc_fix_parse_fields_into(
        (const uint8_t*)msg, strlen(msg), tags, value_offs, value_lens, raw_offs, raw_lens,
        sections, 3, &msg_offset, &msg_length, &checksum, &body_length,
        &declared_checksum, &declared_body_length);

    /* Should parse up to limit */
    ASSERT_EQ(field_count, 3);
}

/*
 * Parse Fields AoS Tests
*/

/* Test fc_fix_parse_fields_aos - NULL checks */
TEST(test_fix_parse_fields_aos_null) {
    const char* msg = "8=FIX.4.4\x01" "9=40\x01" "35=D\x01" "10=000\x01";

    fc_fix_parsed_field_t fields[10];
    uint32_t msg_offset, msg_length, checksum, body_length;
    uint32_t declared_checksum, declared_body_length;

    /* NULL data */
    int status = fc_fix_parse_fields_aos(
        NULL, strlen(msg), fields, 10, &msg_offset, &msg_length,
        &checksum, &body_length, &declared_checksum, &declared_body_length);
    ASSERT_EQ(status, 0);

    /* NULL fields */
    status = fc_fix_parse_fields_aos(
        (const uint8_t*)msg, strlen(msg), NULL, 10, &msg_offset, &msg_length,
        &checksum, &body_length, &declared_checksum, &declared_body_length);
    ASSERT_EQ(status, 0);
}

/* Test fc_fix_parse_fields_aos - valid message */
TEST(test_fix_parse_fields_aos_valid) {
    const char* msg = "8=FIX.4.4\x01" "9=40\x01" "35=D\x01"
                     "49=SENDER\x01" "56=TARGET\x01" "10=000\x01";

    fc_fix_parsed_field_t fields[20];
    uint32_t msg_offset, msg_length, checksum, body_length;
    uint32_t declared_checksum, declared_body_length;

    int field_count = fc_fix_parse_fields_aos(
        (const uint8_t*)msg, strlen(msg), fields, 20, &msg_offset, &msg_length,
        &checksum, &body_length, &declared_checksum, &declared_body_length);

    FC_TEST_ASSERT_NONZERO(field_count);
    FC_TEST_ASSERT_NONZERO(msg_length);

    /* Verify first field is tag 8 */
    ASSERT_EQ(fields[0].tag, 8);
}

/* Test fc_fix_parse_fields_aos - field limit */
TEST(test_fix_parse_fields_aos_limit) {
    const char* msg = "8=FIX.4.4\x01" "9=40\x01" "35=D\x01"
                     "49=SENDER\x01" "56=TARGET\x01" "10=000\x01";

    fc_fix_parsed_field_t fields[3];
    uint32_t msg_offset, msg_length, checksum, body_length;
    uint32_t declared_checksum, declared_body_length;

    int field_count = fc_fix_parse_fields_aos(
        (const uint8_t*)msg, strlen(msg), fields, 3, &msg_offset, &msg_length,
        &checksum, &body_length, &declared_checksum, &declared_body_length);

    ASSERT_EQ(field_count, 3);
}

/*
 * Marshal Message Tests
*/

/* Test FIX message serialization - basic */
TEST(test_fix_marshal_basic) {
    uint8_t buffer[256];

    fc_fix_marshal_field_t fields[] = {
        {35, (const uint8_t*)"D", 1},
        {49, (const uint8_t*)"SENDER", 6},
        {56, (const uint8_t*)"TARGET", 6}
    };

    fc_fix_marshal_result_t result = fc_fix_marshal_message(
        (const uint8_t*)"FIX.4.4", 7,
        fields, 3,
        buffer, sizeof(buffer)
    );

    FC_TEST_ASSERT_NONZERO(result.success);
    FC_TEST_ASSERT_NONZERO(result.output_length);
    FC_TEST_ASSERT_NONZERO(result.body_length);
}

/* Test FIX message serialization - NULL checks */
TEST(test_fix_marshal_null) {
    uint8_t buffer[256];
    fc_fix_marshal_field_t fields[] = {
        {35, (const uint8_t*)"D", 1}
    };

    /* NULL begin_string */
    fc_fix_marshal_result_t result = fc_fix_marshal_message(
        NULL, 7, fields, 1, buffer, sizeof(buffer)
    );
    ASSERT_EQ(result.success, 0);

    /* NULL fields */
    result = fc_fix_marshal_message(
        (const uint8_t*)"FIX.4.4", 7, NULL, 1, buffer, sizeof(buffer)
    );
    ASSERT_EQ(result.success, 0);

    /* NULL output buffer */
    result = fc_fix_marshal_message(
        (const uint8_t*)"FIX.4.4", 7, fields, 1, NULL, sizeof(buffer)
    );
    ASSERT_EQ(result.success, 0);
}

/* Test FIX message serialization - empty fields */
TEST(test_fix_marshal_empty_fields) {
    uint8_t buffer[256];

    fc_fix_marshal_result_t result = fc_fix_marshal_message(
        (const uint8_t*)"FIX.4.4", 7,
        NULL, 0,
        buffer, sizeof(buffer)
    );

    ASSERT_EQ(result.success, 0);
}

/* Test FIX message serialization - buffer too small */
TEST(test_fix_marshal_small_buffer) {
    uint8_t buffer[10];

    fc_fix_marshal_field_t fields[] = {
        {35, (const uint8_t*)"D", 1},
        {49, (const uint8_t*)"SENDER", 6},
        {56, (const uint8_t*)"TARGET", 6}
    };

    fc_fix_marshal_result_t result = fc_fix_marshal_message(
        (const uint8_t*)"FIX.4.4", 7,
        fields, 3,
        buffer, sizeof(buffer)
    );

    ASSERT_EQ(result.success, 0);
}

/* Test FIX message serialization - many fields */
TEST(test_fix_marshal_many_fields) {
    uint8_t buffer[2048];
    fc_fix_marshal_field_t fields[50];

    for (int i = 0; i < 50; i++) {
        fields[i].tag = 100 + i;
        fields[i].value = (const uint8_t*)"TEST";
        fields[i].value_length = 4;
    }

    fc_fix_marshal_result_t result = fc_fix_marshal_message(
        (const uint8_t*)"FIX.4.4", 7,
        fields, 50,
        buffer, sizeof(buffer)
    );

    FC_TEST_ASSERT_NONZERO(result.success);
    FC_TEST_ASSERT_NONZERO(result.output_length);
}

/* Test FIX message serialization - with NULL value */
TEST(test_fix_marshal_null_value) {
    uint8_t buffer[256];

    fc_fix_marshal_field_t fields[] = {
        {35, NULL, 1},
        {49, (const uint8_t*)"SENDER", 6}
    };

    fc_fix_marshal_result_t result = fc_fix_marshal_message(
        (const uint8_t*)"FIX.4.4", 7,
        fields, 2,
        buffer, sizeof(buffer)
    );

    ASSERT_EQ(result.success, 0);
}

/*
 * SIMD Level Test
*/

/* Test fc_fix_simd_level */
TEST(test_fix_simd_level) {
    const char* level = fc_fix_simd_level();
    FC_TEST_ASSERT_NONZERO(level != NULL);

    /* Should be one of the valid SIMD level strings */
    int valid = (strcmp(level, "Scalar (no SIMD)") == 0 ||
                 strcmp(level, "SSE4.2") == 0 ||
                 strcmp(level, "AVX2") == 0 ||
                 strcmp(level, "AVX-512") == 0 ||
                 strcmp(level, "ARM NEON") == 0 ||
                 strcmp(level, "Unknown") == 0);
    FC_TEST_ASSERT_NONZERO(valid);
}

/*
 * Integration Tests
*/

/* Test round-trip: marshal then parse */
TEST(test_fix_roundtrip) {
    uint8_t buffer[256];

    fc_fix_marshal_field_t fields[] = {
        {35, (const uint8_t*)"D", 1},
        {49, (const uint8_t*)"SENDER", 6},
        {56, (const uint8_t*)"TARGET", 6},
        {34, (const uint8_t*)"1", 1}
    };

    fc_fix_marshal_result_t marshal_result = fc_fix_marshal_message(
        (const uint8_t*)"FIX.4.4", 7,
        fields, 4,
        buffer, sizeof(buffer)
    );

    FC_TEST_ASSERT_NONZERO(marshal_result.success);

    /* Now parse it back */
    fc_fix_parse_result_t parse_result;
    int status = fc_fix_parse_messages(buffer, marshal_result.output_length, &parse_result);

    ASSERT_EQ(status, 1);
    ASSERT_EQ(parse_result.msg_count, 1);
}

/* Test parse with offset (message not at start) */
TEST(test_fix_parse_with_offset) {
    /* Message preceded by garbage and SOH delimiter */
    const char* msg = "GARBAGE\x01" "8=FIX.4.4\x01" "9=40\x01" "35=D\x01"
                     "49=SENDER\x01" "56=TARGET\x01" "10=000\x01";

    fc_fix_parse_result_t result;
    int status = fc_fix_parse_messages((const uint8_t*)msg, strlen(msg), &result);

    ASSERT_EQ(status, 1);
    ASSERT_EQ(result.msg_count, 1);
    ASSERT_EQ(result.messages[0].msg_offset, 8);  /* After "GARBAGE\x01" */
}

/* Test parse with false "8" character (not message start) */
TEST(test_fix_parse_false_8) {
    /* Data contains "8" but not "8=" or not preceded by SOH */
    const char* msg = "X8Y\x01" "Z8\x01" "8=FIX.4.4\x01" "9=40\x01" "35=D\x01"
                     "49=SENDER\x01" "56=TARGET\x01" "10=000\x01";

    fc_fix_parse_result_t result;
    int status = fc_fix_parse_messages((const uint8_t*)msg, strlen(msg), &result);

    ASSERT_EQ(status, 1);
    ASSERT_EQ(result.msg_count, 1);
    /* Message should start at "8=FIX.4.4", skipping the false "8" characters */
}

/* Test parse with 4-digit tag */
TEST(test_fix_parse_4digit_tag) {
    /* Use tag 1000 (4 digits) to test fast_parse_tag case 4 */
    const char* msg = "8=FIX.4.4\x01" "9=50\x01" "35=D\x01" "1000=TEST\x01"
                     "49=SENDER\x01" "56=TARGET\x01" "10=000\x01";

    fc_fix_parse_result_t result;
    int status = fc_fix_parse_messages((const uint8_t*)msg, strlen(msg), &result);

    ASSERT_EQ(status, 1);
    ASSERT_EQ(result.msg_count, 1);
}

/* Test parse with 5-digit tag */
TEST(test_fix_parse_5digit_tag) {
    /* Use tag 10000 (5 digits) to test fast_parse_tag default case */
    const char* msg = "8=FIX.4.4\x01" "9=50\x01" "35=D\x01" "10000=TEST\x01"
                     "49=SENDER\x01" "56=TARGET\x01" "10=000\x01";

    fc_fix_parse_result_t result;
    int status = fc_fix_parse_messages((const uint8_t*)msg, strlen(msg), &result);

    ASSERT_EQ(status, 1);
    ASSERT_EQ(result.msg_count, 1);
}

/* Test parse with body fields */
TEST(test_fix_parse_body_fields) {
    /* Message with body fields (after tag 35, before tag 10) */
    const char* msg = "8=FIX.4.4\x01" "9=60\x01" "35=D\x01"
                     "49=SENDER\x01" "56=TARGET\x01" "34=1\x01"
                     "55=AAPL\x01" "54=1\x01" "38=100\x01"  /* Body fields */
                     "10=000\x01";

    fc_fix_parse_result_t result;
    int status = fc_fix_parse_messages((const uint8_t*)msg, strlen(msg), &result);

    ASSERT_EQ(status, 1);
    ASSERT_EQ(result.msg_count, 1);
    /* Should have body_field_count > 0 */
    FC_TEST_ASSERT_NONZERO(result.messages[0].body_field_count > 0);
}

/* Test marshal with insufficient buffer (boundary test) */
TEST(test_fix_marshal_exact_buffer) {
    uint8_t buffer[256];

    fc_fix_marshal_field_t fields[] = {
        {35, (const uint8_t*)"D", 1},
        {49, (const uint8_t*)"SENDER", 6},
        {56, (const uint8_t*)"TARGET", 6}
    };

    /* First, find out how much space we need */
    fc_fix_marshal_result_t result1 = fc_fix_marshal_message(
        (const uint8_t*)"FIX.4.4", 7,
        fields, 3,
        buffer, sizeof(buffer)
    );
    FC_TEST_ASSERT_NONZERO(result1.success);

    /* Try with buffer that's too small - should fail */
    fc_fix_marshal_result_t result2 = fc_fix_marshal_message(
        (const uint8_t*)"FIX.4.4", 7,
        fields, 3,
        buffer, result1.output_length - 1
    );
    ASSERT_EQ(result2.success, 0);

    /* Try with very small buffer - should also fail */
    fc_fix_marshal_result_t result3 = fc_fix_marshal_message(
        (const uint8_t*)"FIX.4.4", 7,
        fields, 3,
        buffer, 10
    );
    ASSERT_EQ(result3.success, 0);
}

/*
 * Test Suite Registration
*/

void register_fix_codec_tests(void) {
    /* Checksum tests */
    RUN_TEST(test_fix_checksum_empty);
    RUN_TEST(test_fix_checksum_null);
    RUN_TEST(test_fix_checksum);
    RUN_TEST(test_fix_checksum_sizes);

    /* Parse messages (batch) tests */
    RUN_TEST(test_fix_parse_null_result);
    RUN_TEST(test_fix_parse_empty);
    RUN_TEST(test_fix_parse_basic);
    RUN_TEST(test_fix_parse_incomplete);
    RUN_TEST(test_fix_parse_multiple);
    RUN_TEST(test_fix_parse_no_start);
    RUN_TEST(test_fix_parse_with_body_length);

    /* Parse one tests */
    RUN_TEST(test_fix_parse_one_null);
    RUN_TEST(test_fix_parse_one_null_data);
    RUN_TEST(test_fix_parse_one_valid);
    RUN_TEST(test_fix_parse_one_incomplete);
    RUN_TEST(test_fix_parse_one_empty);
    RUN_TEST(test_fix_parse_one_many_fields);

    /* Parse fields into (SoA) tests */
    RUN_TEST(test_fix_parse_fields_into_null);
    RUN_TEST(test_fix_parse_fields_into_valid);
    RUN_TEST(test_fix_parse_fields_into_limit);

    /* Parse fields AoS tests */
    RUN_TEST(test_fix_parse_fields_aos_null);
    RUN_TEST(test_fix_parse_fields_aos_valid);
    RUN_TEST(test_fix_parse_fields_aos_limit);

    /* Marshal tests */
    RUN_TEST(test_fix_marshal_basic);
    RUN_TEST(test_fix_marshal_null);
    RUN_TEST(test_fix_marshal_empty_fields);
    RUN_TEST(test_fix_marshal_small_buffer);
    RUN_TEST(test_fix_marshal_many_fields);
    RUN_TEST(test_fix_marshal_null_value);

    /* SIMD level test */
    RUN_TEST(test_fix_simd_level);

    /* Integration tests */
    RUN_TEST(test_fix_roundtrip);
    RUN_TEST(test_fix_parse_with_offset);
    RUN_TEST(test_fix_parse_false_8);
    RUN_TEST(test_fix_parse_4digit_tag);
    RUN_TEST(test_fix_parse_5digit_tag);
    RUN_TEST(test_fix_parse_body_fields);
    RUN_TEST(test_fix_marshal_exact_buffer);
}
