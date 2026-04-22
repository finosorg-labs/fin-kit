/**
 * @file fix_codec.c
 * @brief FIX protocol high-performance codec implementation
 *
 * Optimization strategies:
 *   - CheckSum calculation: AVX2/SSE4.2/scalar runtime dispatch via fin-kit SIMD
 *   - SOH/'=' scanning: memchr (platform C library includes SIMD optimization)
 *   - Tag classification: O(1) bitmap lookup
 *   - Tag parsing: 1-4 digit branch unrolling
 */

#include <codec/fix_codec.h>
#include <platform/simd_detect.h>
#include <platform/platform.h>
#include <string.h>
#include <stdio.h>

#if defined(FC_ARCH_X86) || defined(FC_ARCH_X86_64)
#include <immintrin.h>
#define FC_FIX_SIMD_X86 1
#endif

/* Tag classification bitmaps */
static uint64_t header_tag_bitmap[20];
static uint64_t trailer_tag_bitmap[2];

/* CheckSum function pointer (set during initialization) */
typedef uint32_t (*fc_fix_checksum_fn_t)(const uint8_t* data, size_t length);
static fc_fix_checksum_fn_t g_checksum_impl = NULL;

/* Forward declarations of SIMD implementations */
static uint32_t checksum_scalar(const uint8_t* data, size_t length);
#ifdef FC_FIX_SIMD_X86
__attribute__((target("sse4.2")))
static uint32_t checksum_sse42(const uint8_t* data, size_t length);
__attribute__((target("avx2")))
static uint32_t checksum_avx2(const uint8_t* data, size_t length);
#endif

/**
 * @brief Initialize tag classification bitmaps
 */
static void init_tag_bitmaps(void) {
    static const int header_tags[] = {
        8, 9, 35, 49, 56, 115, 128, 90, 34, 50, 142, 57, 143,
        116, 144, 129, 145, 43, 97, 52, 122, 212, 213, 347,
        369, 789, 370, 1128, 1129, 627, 1156, 91, 628, 629, 630
    };
    static const int trailer_tags[] = { 93, 89, 10 };

    memset(header_tag_bitmap, 0, sizeof(header_tag_bitmap));
    memset(trailer_tag_bitmap, 0, sizeof(trailer_tag_bitmap));

    int n = (int)(sizeof(header_tags) / sizeof(header_tags[0]));
    for (int i = 0; i < n; i++) {
        int t = header_tags[i];
        if (t < 1280)
            header_tag_bitmap[t / 64] |= (1ULL << (t % 64));
    }
    n = (int)(sizeof(trailer_tags) / sizeof(trailer_tags[0]));
    for (int i = 0; i < n; i++) {
        int t = trailer_tags[i];
        if (t < 128)
            trailer_tag_bitmap[t / 64] |= (1ULL << (t % 64));
    }
}

/**
 * @brief Initialize SIMD dispatch based on detected CPU capabilities
 */
static void init_simd_dispatch(void) {
    fc_simd_level_t level = fc_get_simd_level();

#ifdef FC_FIX_SIMD_X86
    if (level >= FC_SIMD_AVX2) {
        g_checksum_impl = checksum_avx2;
    } else if (level >= FC_SIMD_SSE42) {
        g_checksum_impl = checksum_sse42;
    } else {
        g_checksum_impl = checksum_scalar;
    }
#else
    (void)level;
    g_checksum_impl = checksum_scalar;
#endif
}

/**
 * @brief One-time initialization (called by fc_init)
 */
void fc_fix_codec_init(void) {
    init_tag_bitmaps();
    init_simd_dispatch();
}

/**
 * @brief Classify tag into header/body/trailer section
 */
static inline uint8_t classify_tag(int32_t tag) {
    if (tag <= 0)
        return FC_FIX_SECTION_BODY;
    if (tag < 1280 && (header_tag_bitmap[tag / 64] & (1ULL << (tag % 64))))
        return FC_FIX_SECTION_HEADER;
    if (tag < 128 && (trailer_tag_bitmap[tag / 64] & (1ULL << (tag % 64))))
        return FC_FIX_SECTION_TRAILER;
    return FC_FIX_SECTION_BODY;
}

/**
 * @brief Fast integer parsing with branch unrolling
 */
static inline int32_t fast_parse_tag(const uint8_t* data, int len) {
    switch (len) {
    case 1: return data[0] - '0';
    case 2: return (data[0] - '0') * 10 + (data[1] - '0');
    case 3: return (data[0] - '0') * 100 + (data[1] - '0') * 10 + (data[2] - '0');
    case 4: return (data[0] - '0') * 1000 + (data[1] - '0') * 100 +
                   (data[2] - '0') * 10 + (data[3] - '0');
    default: {
        int32_t n = 0;
        for (int i = 0; i < len; i++)
            n = n * 10 + (data[i] - '0');
        return n;
    }
    }
}

static inline uint32_t fast_parse_uint(const uint8_t* data, int len) {
    uint32_t n = 0;
    for (int i = 0; i < len; i++)
        n = n * 10 + (data[i] - '0');
    return n;
}

/**
 * @brief Get required tag bit for validation
 */
static inline uint32_t get_required_tag_bit(int32_t tag) {
    switch (tag) {
    case 8:  return FC_FIX_REQBIT_BEGINSTRING;
    case 9:  return FC_FIX_REQBIT_BODYLENGTH;
    case 35: return FC_FIX_REQBIT_MSGTYPE;
    case 34: return FC_FIX_REQBIT_MSGSEQNUM;
    case 49: return FC_FIX_REQBIT_SENDERCOMPID;
    case 56: return FC_FIX_REQBIT_TARGETCOMPID;
    case 52: return FC_FIX_REQBIT_SENDINGTIME;
    case 10: return FC_FIX_REQBIT_CHECKSUM;
    default: return 0;
    }
}

/*
 * SIMD CheckSum Implementations
*/

/**
 * @brief Scalar checksum implementation (fallback for all platforms)
 */
static uint32_t checksum_scalar(const uint8_t* data, size_t length) {
    uint32_t sum = 0;
    size_t i = 0;

    /* 8-byte unrolling for ILP */
    for (; i + 8 <= length; i += 8) {
        sum += data[i]   + data[i+1] + data[i+2] + data[i+3]
             + data[i+4] + data[i+5] + data[i+6] + data[i+7];
    }
    for (; i < length; i++)
        sum += data[i];

    return sum;
}

#ifdef FC_FIX_SIMD_X86

/**
 * @brief SSE4.2 checksum implementation
 *
 * Uses _mm_sad_epu8 to process 16 bytes per iteration.
 * SSE4.2 is part of x86-64 baseline ISA.
 */
__attribute__((target("sse4.2")))
static uint32_t checksum_sse42(const uint8_t* data, size_t length) {
    __m128i acc = _mm_setzero_si128();
    __m128i zero = _mm_setzero_si128();
    size_t i = 0;

    /* Process 16 bytes per iteration */
    for (; i + 16 <= length; i += 16) {
        __m128i chunk = _mm_loadu_si128((const __m128i*)(data + i));
        acc = _mm_add_epi64(acc, _mm_sad_epu8(chunk, zero));
    }

    /* Horizontal reduction: sum two 64-bit lanes */
    uint64_t tmp[2];
    _mm_storeu_si128((__m128i*)tmp, acc);
    uint32_t sum = (uint32_t)(tmp[0] + tmp[1]);

    /* Process tail */
    for (; i < length; i++)
        sum += data[i];

    return sum;
}

/**
 * @brief AVX2 checksum implementation
 *
 * Uses _mm256_sad_epu8 to process 32 bytes per iteration.
 * Approximately 2x throughput of SSE4.2.
 */
__attribute__((target("avx2")))
static uint32_t checksum_avx2(const uint8_t* data, size_t length) {
    __m256i acc = _mm256_setzero_si256();
    __m256i zero = _mm256_setzero_si256();
    size_t i = 0;

    /* Process 32 bytes per iteration */
    for (; i + 32 <= length; i += 32) {
        __m256i chunk = _mm256_loadu_si256((const __m256i*)(data + i));
        acc = _mm256_add_epi64(acc, _mm256_sad_epu8(chunk, zero));
    }

    /* Horizontal reduction: sum four 64-bit lanes */
    uint64_t tmp[4];
    _mm256_storeu_si256((__m256i*)tmp, acc);
    uint32_t sum = (uint32_t)(tmp[0] + tmp[1] + tmp[2] + tmp[3]);

    /* Process tail (< 32 bytes) */
    for (; i < length; i++)
        sum += data[i];

    return sum;
}

#endif /* FC_FIX_SIMD_X86 */

/*
 * Public API
*/

uint32_t fc_fix_checksum(const uint8_t* data, size_t length) {
    if (!data || length == 0) {
        return 0;
    }
    if (!g_checksum_impl) {
        init_simd_dispatch();
    }
    return g_checksum_impl(data, length);
}

int fc_fix_parse_messages(
    const uint8_t*         data,
    size_t                 data_length,
    fc_fix_parse_result_t* result)
{
    if (!result) return FC_ERR_INVALID_ARG;

    result->msg_count      = 0;
    result->consumed_bytes = 0;

    size_t pos = 0;

    while (pos < data_length && result->msg_count < FC_FIX_MAX_MESSAGES) {
        /* Step 1: Find message start "8=" pattern using memchr */
        size_t msg_start = (size_t)-1;
        {
            const uint8_t* p = data + pos;
            const uint8_t* end = data + data_length;
            while (p + 1 < end) {
                const uint8_t* found = (const uint8_t*)memchr(p, '8', (size_t)(end - 1 - p));
                if (!found) break;
                if (found[1] == '=' && (found == data || found[-1] == FC_FIX_SOH)) {
                    msg_start = (size_t)(found - data);
                    break;
                }
                p = found + 1;
            }
        }
        if (msg_start == (size_t)-1) break;

        /* Initialize current message parse result */
        fc_fix_parsed_message_t* pm = &result->messages[result->msg_count];
        memset(pm, 0, sizeof(fc_fix_parsed_message_t));
        pm->msg_offset = (uint32_t)msg_start;

        size_t field_start       = msg_start;
        size_t body_start_pos    = 0;
        int    found_body_length = 0;
        int    found_end         = 0;

        /* Step 2: Parse fields */
        while (field_start < data_length) {
            /* Find SOH using memchr (platform C library has SIMD optimization) */
            const uint8_t* soh_ptr = (const uint8_t*)memchr(
                data + field_start, FC_FIX_SOH, data_length - field_start);
            if (!soh_ptr) break;
            size_t soh = (size_t)(soh_ptr - data);

            /* Find '=' separator using memchr */
            const uint8_t* eq_ptr = (const uint8_t*)memchr(
                data + field_start, '=', soh - field_start);
            if (!eq_ptr) { field_start = soh + 1; continue; }
            size_t eq = (size_t)(eq_ptr - data);

            int     tag_len = (int)(eq - field_start);
            int32_t tag     = fast_parse_tag(data + field_start, tag_len);

            /* All offsets are relative to message start */
            uint32_t val_off = (uint32_t)(eq + 1 - msg_start);
            uint16_t val_len = (uint16_t)(soh - eq - 1);
            uint32_t raw_off = (uint32_t)(field_start - msg_start);
            uint16_t raw_len = (uint16_t)(soh + 1 - field_start);
            uint8_t  section = classify_tag(tag);

            if (pm->field_count < FC_FIX_MAX_FIELDS_PER_MSG) {
                fc_fix_parsed_field_t* pf = &pm->fields[pm->field_count];
                pf->tag          = tag;
                pf->value_offset = val_off;
                pf->value_length = val_len;
                pf->raw_offset   = raw_off;
                pf->raw_length   = raw_len;
                pf->section      = section;
                pm->field_count++;

                switch (section) {
                case FC_FIX_SECTION_HEADER:  pm->header_field_count++;  break;
                case FC_FIX_SECTION_BODY:    pm->body_field_count++;    break;
                case FC_FIX_SECTION_TRAILER: pm->trailer_field_count++; break;
                }
            }

            pm->required_tags_bitmap |= get_required_tag_bit(tag);

            if (tag == 9 && !found_body_length) {
                body_start_pos           = soh + 1;
                pm->declared_body_length = fast_parse_uint(data + eq + 1, (int)(soh - eq - 1));
                found_body_length        = 1;
            }

            if (tag == 10) {
                pm->declared_checksum = fast_parse_uint(data + eq + 1, (int)(soh - eq - 1));
                pm->msg_length = (uint32_t)(soh + 1 - msg_start);

                if (found_body_length)
                    pm->computed_body_length = (uint32_t)(field_start - body_start_pos);

                /* CheckSum using SIMD-accelerated implementation */
                pm->computed_checksum = fc_fix_checksum(data + msg_start,
                                                        field_start - msg_start);

                found_end = 1;
                result->msg_count++;
                result->consumed_bytes = (int32_t)(soh + 1);
                pos = soh + 1;
                break;
            }

            field_start = soh + 1;
        }

        if (!found_end) break;
    }

    return result->msg_count;
}

fc_fix_marshal_result_t fc_fix_marshal_message(
    const uint8_t*                begin_string,
    int32_t                       begin_string_len,
    const fc_fix_marshal_field_t* fields,
    int32_t                       field_count,
    uint8_t*                      output_buf,
    size_t                        output_buf_cap)
{
    fc_fix_marshal_result_t result;
    memset(&result, 0, sizeof(result));

    if (!output_buf || output_buf_cap < 64 || !begin_string || !fields) {
        return result;
    }

    /* Reserve space for header (8= and 9=) */
    const size_t reserve = 32;
    uint8_t* body_ptr = output_buf + reserve;
    uint8_t* p = body_ptr;
    size_t   remaining = output_buf_cap - reserve - 16;

    /* Write body fields */
    for (int32_t i = 0; i < field_count; i++) {
        /* Write "tag=" */
        char tmp[12];
        int tlen = snprintf(tmp, sizeof(tmp), "%d=", (int)fields[i].tag);
        if ((size_t)(p - output_buf) + tlen + fields[i].value_length + 1 >= remaining) {
            return result;
        }
        memcpy(p, tmp, (size_t)tlen);
        p += tlen;

        /* Write value */
        if (fields[i].value_length > 0) {
            if (!fields[i].value) {
                return result;  /* NULL value pointer is invalid */
            }
            memcpy(p, fields[i].value, fields[i].value_length);
            p += fields[i].value_length;
        }

        /* Write SOH */
        *p++ = FC_FIX_SOH;
    }

    uint32_t body_length = (uint32_t)(p - body_ptr);

    /* Build header prefix */
    uint8_t prefix[48];
    int plen = 0;

    /* 8=BeginString */
    plen += snprintf((char*)prefix + plen, sizeof(prefix) - plen, "8=");
    memcpy(prefix + plen, begin_string, (size_t)begin_string_len);
    plen += begin_string_len;
    prefix[plen++] = FC_FIX_SOH;

    /* 9=BodyLength */
    plen += snprintf((char*)prefix + plen, sizeof(prefix) - plen, "9=%u%c",
                     (unsigned)body_length, FC_FIX_SOH);

    /* Move body and insert header */
    memmove(output_buf + plen, body_ptr, body_length);
    memcpy(output_buf, prefix, (size_t)plen);

    uint32_t total = (uint32_t)plen + body_length;

    /* Calculate checksum using SIMD-accelerated implementation */
    uint32_t cksum = fc_fix_checksum(output_buf, total) % 256;

    /* Write checksum trailer */
    int tail = snprintf((char*)output_buf + total, output_buf_cap - total,
                       "10=%03u%c", (unsigned)cksum, FC_FIX_SOH);
    if (tail < 0 || (size_t)(total + tail) > output_buf_cap) {
        return result;
    }
    total += (uint32_t)tail;

    result.success       = 1;
    result.output_length = total;
    result.body_length   = body_length;
    result.checksum      = cksum;
    return result;
}

/*
 * Single Message Parsing (Lightweight)
*/

int fc_fix_parse_one(
    const uint8_t*                data,
    size_t                        data_length,
    fc_fix_parse_single_result_t* result)
{
    if (!data || !result) {
        return 0;
    }

    /* Initialize result */
    result->success              = 0;
    result->msg_offset           = 0;
    result->msg_length           = 0;
    result->computed_checksum    = 0;
    result->computed_body_length = 0;
    result->declared_checksum    = 0;
    result->declared_body_length = 0;
    result->field_count          = 0;
    result->header_field_count   = 0;
    result->body_field_count     = 0;
    result->trailer_field_count  = 0;
    result->required_tags_bitmap = 0;

    /* Find message start (8=) */
    const uint8_t* msg_start = NULL;
    {
        const uint8_t* p = data;
        const uint8_t* end = data + data_length;
        while (p + 1 < end) {
            const uint8_t* found = (const uint8_t*)memchr(p, '8', (size_t)(end - 1 - p));
            if (!found) break;
            if (found[1] == '=' && (found == data || found[-1] == FC_FIX_SOH)) {
                msg_start = found;
                break;
            }
            p = found + 1;
        }
    }
    if (!msg_start) return 0;

    result->msg_offset = (uint32_t)(msg_start - data);

    /* Parse fields */
    const uint8_t* field_start       = msg_start;
    const uint8_t* body_start_pos    = NULL;
    int            found_body_length = 0;
    int32_t        field_count       = 0;

    while (field_start < data + data_length) {
        const uint8_t* soh_ptr = (const uint8_t*)memchr(
            field_start, FC_FIX_SOH, (size_t)(data + data_length - field_start));
        if (!soh_ptr) break;

        const uint8_t* eq_ptr = (const uint8_t*)memchr(
            field_start, '=', (size_t)(soh_ptr - field_start));
        if (!eq_ptr) { field_start = soh_ptr + 1; continue; }

        int     tag_len = (int)(eq_ptr - field_start);
        int32_t tag     = fast_parse_tag(field_start, tag_len);

        if (field_count < FC_FIX_SINGLE_MAX_FIELDS) {
            fc_fix_parsed_field_t* pf = &result->fields[field_count];
            pf->tag          = tag;
            pf->value_offset = (uint32_t)(eq_ptr + 1 - msg_start);
            pf->value_length = (uint16_t)(soh_ptr - eq_ptr - 1);
            pf->raw_offset   = (uint32_t)(field_start - msg_start);
            pf->raw_length   = (uint16_t)(soh_ptr + 1 - field_start);
            pf->section      = classify_tag(tag);

            /* Update section counts */
            if (pf->section == FC_FIX_SECTION_HEADER) {
                result->header_field_count++;
            } else if (pf->section == FC_FIX_SECTION_BODY) {
                result->body_field_count++;
            } else if (pf->section == FC_FIX_SECTION_TRAILER) {
                result->trailer_field_count++;
            }

            field_count++;
        }

        if (tag == 9 && !found_body_length) {
            body_start_pos                = soh_ptr + 1;
            result->declared_body_length  = fast_parse_uint(eq_ptr + 1, (int)(soh_ptr - eq_ptr - 1));
            found_body_length             = 1;
        }

        if (tag == 10) {
            result->declared_checksum    = fast_parse_uint(eq_ptr + 1, (int)(soh_ptr - eq_ptr - 1));
            result->msg_length           = (uint32_t)(soh_ptr + 1 - msg_start);
            result->computed_body_length = found_body_length ? (uint32_t)(field_start - body_start_pos) : 0;
            result->computed_checksum    = fc_fix_checksum(msg_start, (size_t)(field_start - msg_start));
            result->field_count          = field_count;
            result->success              = 1;
            return 1;
        }

        field_start = soh_ptr + 1;
    }

    return 0;
}

/*
 * SoA Field Parsing
*/

int fc_fix_parse_fields_into(
    const uint8_t*  data,
    size_t          data_length,
    int32_t*        out_tags,
    uint32_t*       out_value_offs,
    uint16_t*       out_value_lens,
    uint32_t*       out_raw_offs,
    uint16_t*       out_raw_lens,
    uint8_t*        out_sections,
    int             max_fields,
    uint32_t*       out_msg_offset,
    uint32_t*       out_msg_length,
    uint32_t*       out_checksum,
    uint32_t*       out_body_length,
    uint32_t*       out_declared_checksum,
    uint32_t*       out_declared_body_length)
{
    if (!data || !out_tags || !out_value_offs || !out_value_lens ||
        !out_raw_offs || !out_raw_lens || !out_sections ||
        !out_msg_offset || !out_msg_length || !out_checksum ||
        !out_body_length || !out_declared_checksum || !out_declared_body_length) {
        return 0;
    }

    /* Find message start (8=) */
    size_t msg_start = (size_t)-1;
    {
        const uint8_t* p = data;
        const uint8_t* end = data + data_length;
        while (p + 1 < end) {
            const uint8_t* found = (const uint8_t*)memchr(p, '8', (size_t)(end - 1 - p));
            if (!found) break;
            if (found[1] == '=' && (found == data || found[-1] == FC_FIX_SOH)) {
                msg_start = (size_t)(found - data);
                break;
            }
            p = found + 1;
        }
    }
    if (msg_start == (size_t)-1) return 0;

    *out_msg_offset = (uint32_t)msg_start;

    size_t field_start       = msg_start;
    size_t body_start_pos    = 0;
    int    found_body_length = 0;
    int32_t field_count      = 0;

    while (field_start < data_length) {
        const uint8_t* soh_ptr = (const uint8_t*)memchr(
            data + field_start, FC_FIX_SOH, data_length - field_start);
        if (!soh_ptr) break;
        size_t soh = (size_t)(soh_ptr - data);

        const uint8_t* eq_ptr = (const uint8_t*)memchr(
            data + field_start, '=', soh - field_start);
        if (!eq_ptr) { field_start = soh + 1; continue; }
        size_t eq = (size_t)(eq_ptr - data);

        int     tag_len = (int)(eq - field_start);
        int32_t tag     = fast_parse_tag(data + field_start, tag_len);

        if (field_count < max_fields) {
            out_tags[field_count]        = tag;
            out_value_offs[field_count]  = (uint32_t)(eq + 1 - msg_start);
            out_value_lens[field_count]  = (uint16_t)(soh - eq - 1);
            out_raw_offs[field_count]    = (uint32_t)(field_start - msg_start);
            out_raw_lens[field_count]    = (uint16_t)(soh + 1 - field_start);
            out_sections[field_count]    = classify_tag(tag);
            field_count++;
        }

        if (tag == 9 && !found_body_length) {
            body_start_pos                = soh + 1;
            *out_declared_body_length     = fast_parse_uint(data + eq + 1, (int)(soh - eq - 1));
            found_body_length             = 1;
        }

        if (tag == 10) {
            *out_declared_checksum = fast_parse_uint(data + eq + 1, (int)(soh - eq - 1));
            *out_msg_length        = (uint32_t)(soh + 1 - msg_start);
            *out_body_length       = found_body_length ? (uint32_t)(field_start - body_start_pos) : 0;
            *out_checksum          = fc_fix_checksum(data + msg_start, field_start - msg_start);
            return field_count;
        }

        field_start = soh + 1;
    }

    return 0;
}

/*
 * AoS Field Parsing
*/

int fc_fix_parse_fields_aos(
    const uint8_t*            data,
    size_t                    data_length,
    fc_fix_parsed_field_t*    out_fields,
    int                       max_fields,
    uint32_t*                 out_msg_offset,
    uint32_t*                 out_msg_length,
    uint32_t*                 out_checksum,
    uint32_t*                 out_body_length,
    uint32_t*                 out_declared_checksum,
    uint32_t*                 out_declared_body_length)
{
    if (!data || !out_fields || !out_msg_offset || !out_msg_length ||
        !out_checksum || !out_body_length || !out_declared_checksum ||
        !out_declared_body_length) {
        return 0;
    }

    /* Find message start (8=) */
    size_t msg_start = (size_t)-1;
    {
        const uint8_t* p = data;
        const uint8_t* end = data + data_length;
        while (p + 1 < end) {
            const uint8_t* found = (const uint8_t*)memchr(p, '8', (size_t)(end - 1 - p));
            if (!found) break;
            if (found[1] == '=' && (found == data || found[-1] == FC_FIX_SOH)) {
                msg_start = (size_t)(found - data);
                break;
            }
            p = found + 1;
        }
    }
    if (msg_start == (size_t)-1) return 0;

    *out_msg_offset = (uint32_t)msg_start;

    size_t field_start       = msg_start;
    size_t body_start_pos    = 0;
    int    found_body_length = 0;
    int32_t field_count      = 0;

    while (field_start < data_length) {
        const uint8_t* soh_ptr = (const uint8_t*)memchr(
            data + field_start, FC_FIX_SOH, data_length - field_start);
        if (!soh_ptr) break;
        size_t soh = (size_t)(soh_ptr - data);

        const uint8_t* eq_ptr = (const uint8_t*)memchr(
            data + field_start, '=', soh - field_start);
        if (!eq_ptr) { field_start = soh + 1; continue; }
        size_t eq = (size_t)(eq_ptr - data);

        int     tag_len = (int)(eq - field_start);
        int32_t tag     = fast_parse_tag(data + field_start, tag_len);

        if (field_count < max_fields) {
            fc_fix_parsed_field_t* pf = &out_fields[field_count];
            pf->tag          = tag;
            pf->value_offset = (uint32_t)(eq + 1 - msg_start);
            pf->value_length = (uint16_t)(soh - eq - 1);
            pf->raw_offset   = (uint32_t)(field_start - msg_start);
            pf->raw_length   = (uint16_t)(soh + 1 - field_start);
            pf->section      = classify_tag(tag);
            field_count++;
        }

        if (tag == 9 && !found_body_length) {
            body_start_pos            = soh + 1;
            *out_declared_body_length = fast_parse_uint(data + eq + 1, (int)(soh - eq - 1));
            found_body_length         = 1;
        }

        if (tag == 10) {
            *out_declared_checksum = fast_parse_uint(data + eq + 1, (int)(soh - eq - 1));
            *out_msg_length        = (uint32_t)(soh + 1 - msg_start);
            *out_body_length       = found_body_length ? (uint32_t)(field_start - body_start_pos) : 0;
            *out_checksum          = fc_fix_checksum(data + msg_start, field_start - msg_start);
            return field_count;
        }

        field_start = soh + 1;
    }

    return 0;
}

/*
 * SIMD Diagnostic
*/

const char* fc_fix_simd_level(void)
{
    fc_simd_level_t level = fc_get_simd_level();
    return fc_simd_level_string(level);
}
