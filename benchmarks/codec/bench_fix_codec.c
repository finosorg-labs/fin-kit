/**
 * @file bench_fix_codec.c
 * @brief Performance benchmarks for FIX protocol codec
 *
 * Measures FIX message parsing, marshaling, and checksum calculation
 * at various message sizes and batch sizes.
 * Reports throughput in messages/sec and MB/s.
 */

#include "bench_framework.h"
#include <fin-kit/codec/fix_codec.h>
#include <fin-kit/platform/simd_detect.h>
#include <fin-kit/platform/platform.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Sample FIX messages of various sizes */

/* Small message: ~50 bytes, 6 fields */
static const char* SMALL_FIX_MSG =
    "8=FIX.4.4\x01"
    "9=25\x01"
    "35=D\x01"
    "49=SENDER\x01"
    "56=TARGET\x01"
    "10=197\x01";

/* Medium message: ~150 bytes, 15 fields */
static const char* MEDIUM_FIX_MSG =
    "8=FIX.4.4\x01"
    "9=120\x01"
    "35=D\x01"
    "49=SENDER\x01"
    "56=TARGET\x01"
    "34=1\x01"
    "52=20250101-12:00:00\x01"
    "11=ORDER123\x01"
    "21=1\x01"
    "55=AAPL\x01"
    "54=1\x01"
    "60=20250101-12:00:00\x01"
    "38=100\x01"
    "40=2\x01"
    "44=150.50\x01"
    "10=123\x01";

/* Large message: ~300 bytes, 25 fields */
static const char* LARGE_FIX_MSG =
    "8=FIX.4.4\x01"
    "9=250\x01"
    "35=D\x01"
    "49=SENDER\x01"
    "56=TARGET\x01"
    "34=1\x01"
    "52=20250101-12:00:00.000\x01"
    "11=ORDER123456789\x01"
    "21=1\x01"
    "55=AAPL\x01"
    "54=1\x01"
    "60=20250101-12:00:00.000\x01"
    "38=100\x01"
    "40=2\x01"
    "44=150.50\x01"
    "59=0\x01"
    "100=NASDAQ\x01"
    "15=USD\x01"
    "1=ACCOUNT123\x01"
    "47=A\x01"
    "110=0\x01"
    "111=100\x01"
    "526=ORDER_ID_123\x01"
    "528=P\x01"
    "581=1\x01"
    "10=123\x01";

/* Benchmark data structures */

typedef struct {
    const uint8_t* data;
    uint32_t length;
} checksum_data_t;

typedef struct {
    const uint8_t* data;
    uint32_t length;
    fc_fix_parsed_message_t* messages;
    int max_messages;
} parse_messages_data_t;

typedef struct {
    const uint8_t* data;
    uint32_t length;
    fc_fix_parse_single_result_t* result;
} parse_one_data_t;

typedef struct {
    const char* begin_string;
    fc_fix_marshal_field_t* fields;
    int field_count;
    uint8_t* buffer;
    uint32_t buffer_size;
} marshal_data_t;

/* Helper: create a batch of messages */
static uint8_t* create_message_batch(const char* msg, int count, uint32_t* out_length) {
    uint32_t msg_len = (uint32_t)strlen(msg);
    uint32_t total_len = msg_len * count;
    uint8_t* batch = (uint8_t*)malloc(total_len);

    for (int i = 0; i < count; i++) {
        memcpy(batch + i * msg_len, msg, msg_len);
    }

    *out_length = total_len;
    return batch;
}

/* Helper: create fields for marshaling */
static fc_fix_marshal_field_t* create_test_fields(int* out_count) {
    static fc_fix_marshal_field_t fields[6];

    fields[0].tag = 35;
    fields[0].value = (const uint8_t*)"D";
    fields[0].value_length = 1;

    fields[1].tag = 49;
    fields[1].value = (const uint8_t*)"SENDER";
    fields[1].value_length = 6;

    fields[2].tag = 56;
    fields[2].value = (const uint8_t*)"TARGET";
    fields[2].value_length = 6;

    fields[3].tag = 11;
    fields[3].value = (const uint8_t*)"ORDER123";
    fields[3].value_length = 8;

    fields[4].tag = 55;
    fields[4].value = (const uint8_t*)"AAPL";
    fields[4].value_length = 4;

    fields[5].tag = 38;
    fields[5].value = (const uint8_t*)"100";
    fields[5].value_length = 3;

    *out_count = 6;
    return fields;
}

/* Benchmark functions */

static void bench_checksum_fn(void* user_data) {
    checksum_data_t* d = (checksum_data_t*)user_data;
    (void)fc_fix_checksum(d->data, d->length);
}

static void bench_parse_messages_fn(void* user_data) {
    parse_messages_data_t* d = (parse_messages_data_t*)user_data;
    fc_fix_parse_result_t result;
    fc_fix_parse_messages(d->data, d->length, &result);
}

static void bench_parse_one_fn(void* user_data) {
    parse_one_data_t* d = (parse_one_data_t*)user_data;
    fc_fix_parse_one(d->data, d->length, d->result);
}

static void bench_marshal_fn(void* user_data) {
    marshal_data_t* d = (marshal_data_t*)user_data;
    fc_fix_marshal_result_t result = fc_fix_marshal_message(
        (const uint8_t*)d->begin_string, (int32_t)strlen(d->begin_string),
        d->fields, d->field_count,
        d->buffer, d->buffer_size);
    (void)result; // Suppress unused warning
}

/* Run checksum benchmarks */
static void run_checksum_benchmarks(void) {
    printf("\n");
    printf("FIX Checksum Benchmarks (SIMD: %s)\n", fc_simd_level_string(fc_get_simd_level()));
    printf("------------------------------------------------------------\n");

    struct {
        const char* name;
        const char* msg;
        int count;
    } tests[] = {
        {"Checksum/Small/50B", SMALL_FIX_MSG, 1},
        {"Checksum/Medium/150B", MEDIUM_FIX_MSG, 1},
        {"Checksum/Large/300B", LARGE_FIX_MSG, 1},
        {"Checksum/Batch10x", SMALL_FIX_MSG, 10},
        {"Checksum/Batch100x", SMALL_FIX_MSG, 100},
        {"Checksum/Batch1000x", SMALL_FIX_MSG, 1000},
    };

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        uint32_t length;
        uint8_t* data = create_message_batch(tests[i].msg, tests[i].count, &length);

        checksum_data_t bench_data = {
            .data = data,
            .length = length
        };

        fc_bench_config_t config = FC_BENCH_CONFIG_DEFAULT;
        config.name = tests[i].name;
        config.data_size = length;
        config.min_time_ms = 50.0;
        config.quiet = 0;

        fc_bench_result_t result;
        fc_bench_run(&config, bench_checksum_fn, &bench_data, &result);

        free(data);
    }
}

/* Run parse messages benchmarks */
static void run_parse_messages_benchmarks(void) {
    printf("\n");
    printf("FIX Parse Messages Benchmarks (Batch Parsing)\n");
    printf("------------------------------------------------------------\n");

    struct {
        const char* name;
        const char* msg;
        int count;
    } tests[] = {
        {"ParseMessages/1msg", SMALL_FIX_MSG, 1},
        {"ParseMessages/10msg", SMALL_FIX_MSG, 10},
        {"ParseMessages/100msg", SMALL_FIX_MSG, 100},
        {"ParseMessages/1000msg", SMALL_FIX_MSG, 1000},
        {"ParseMessages/Medium100x", MEDIUM_FIX_MSG, 100},
        {"ParseMessages/Large100x", LARGE_FIX_MSG, 100},
    };

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        uint32_t length;
        uint8_t* data = create_message_batch(tests[i].msg, tests[i].count, &length);

        fc_fix_parsed_message_t* messages =
            (fc_fix_parsed_message_t*)malloc(tests[i].count * sizeof(fc_fix_parsed_message_t));

        parse_messages_data_t bench_data = {
            .data = data,
            .length = length,
            .messages = messages,
            .max_messages = tests[i].count
        };

        fc_bench_config_t config = FC_BENCH_CONFIG_DEFAULT;
        config.name = tests[i].name;
        config.data_size = length;
        config.min_time_ms = 50.0;
        config.quiet = 0;

        fc_bench_result_t result;
        fc_bench_run(&config, bench_parse_messages_fn, &bench_data, &result);

        free(messages);
        free(data);
    }
}

/* Run parse one benchmarks */
static void run_parse_one_benchmarks(void) {
    printf("\n");
    printf("FIX Parse One Benchmarks (Single Message Parsing)\n");
    printf("------------------------------------------------------------\n");

    struct {
        const char* name;
        const char* msg;
    } tests[] = {
        {"ParseOne/Small/6fields", SMALL_FIX_MSG},
        {"ParseOne/Medium/15fields", MEDIUM_FIX_MSG},
        {"ParseOne/Large/25fields", LARGE_FIX_MSG},
    };

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        uint32_t length = (uint32_t)strlen(tests[i].msg);
        fc_fix_parse_single_result_t result_data;

        parse_one_data_t bench_data = {
            .data = (const uint8_t*)tests[i].msg,
            .length = length,
            .result = &result_data
        };

        fc_bench_config_t config = FC_BENCH_CONFIG_DEFAULT;
        config.name = tests[i].name;
        config.data_size = length;
        config.min_time_ms = 50.0;
        config.quiet = 0;

        fc_bench_result_t result;
        fc_bench_run(&config, bench_parse_one_fn, &bench_data, &result);
    }
}

/* Run marshal benchmarks */
static void run_marshal_benchmarks(void) {
    printf("\n");
    printf("FIX Marshal Benchmarks (Message Serialization)\n");
    printf("------------------------------------------------------------\n");

    int field_count;
    fc_fix_marshal_field_t* fields = create_test_fields(&field_count);
    uint8_t buffer[1024];

    marshal_data_t bench_data = {
        .begin_string = "FIX.4.4",
        .fields = fields,
        .field_count = field_count,
        .buffer = buffer,
        .buffer_size = sizeof(buffer)
    };

    fc_bench_config_t config = FC_BENCH_CONFIG_DEFAULT;
    config.name = "Marshal/6fields";
    config.data_size = 100;  /* approximate output size */
    config.min_time_ms = 50.0;
    config.quiet = 0;

    fc_bench_result_t result;
    fc_bench_run(&config, bench_marshal_fn, &bench_data, &result);
}

/* Run roundtrip benchmarks (parse + marshal) */
static void run_roundtrip_benchmarks(void) {
    printf("\n");
    printf("FIX Roundtrip Benchmarks (Parse + Marshal)\n");
    printf("------------------------------------------------------------\n");

    const char* msg = SMALL_FIX_MSG;
    uint32_t msg_len = (uint32_t)strlen(msg);

    fc_bench_time_t start = fc_bench_time_now();
    int iterations = 10000;

    fc_fix_parse_single_result_t parse_result;
    uint8_t marshal_buffer[1024];

    for (int i = 0; i < iterations; i++) {
        /* Parse */
        int ret = fc_fix_parse_one((const uint8_t*)msg, msg_len, &parse_result);
        if (ret != 0) continue;

        /* Marshal */
        fc_fix_marshal_field_t fields[FC_FIX_MAX_FIELDS_PER_MSG];
        int field_count = 0;

        for (int j = 0; j < parse_result.field_count && j < FC_FIX_MAX_FIELDS_PER_MSG; j++) {
            if (parse_result.fields[j].tag != 8 &&
                parse_result.fields[j].tag != 9 &&
                parse_result.fields[j].tag != 10) {
                fields[field_count].tag = parse_result.fields[j].tag;
                fields[field_count].value = (const uint8_t*)msg + parse_result.fields[j].value_offset;
                fields[field_count].value_length = parse_result.fields[j].value_length;
                field_count++;
            }
        }

        fc_fix_marshal_result_t marshal_result = fc_fix_marshal_message(
            (const uint8_t*)"FIX.4.4", 7,
            fields, field_count,
            marshal_buffer, sizeof(marshal_buffer));
        (void)marshal_result; // Suppress unused warning
    }

    fc_bench_time_t end = fc_bench_time_now();
    double elapsed_ms = fc_bench_time_elapsed_ms(&start, &end);
    double ns_per_op = (elapsed_ms * 1000000.0) / iterations;

    printf("%-50s\t%10d\t%12.2f ns/op\n",
           "Roundtrip/ParseMarshal",
           iterations,
           ns_per_op);
}

/**
 * @brief Run all FIX codec benchmarks
 */
void bench_codec_run(void) {
    printf("\n");
    printf("FIX Codec Benchmarks\n");
    printf("Platform: %s, Arch: %s, SIMD: %s\n",
           FC_OS_STRING,
           FC_ARCH_STRING,
           fc_simd_level_string(fc_get_simd_level()));
    printf("------------------------------------------------------------\n");

    run_checksum_benchmarks();
    run_parse_messages_benchmarks();
    run_parse_one_benchmarks();
    run_marshal_benchmarks();
    run_roundtrip_benchmarks();
}
