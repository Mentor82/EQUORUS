#ifndef EQUORUS_C_H
#define EQUORUS_C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EQUORUS_C_ABI_VERSION 1u

typedef enum equorus_status {
    EQUORUS_STATUS_OK = 0,
    EQUORUS_STATUS_INVALID_ARGUMENT = 1,
    EQUORUS_STATUS_LIMIT = 2,
    EQUORUS_STATUS_SCHEMA = 3,
    EQUORUS_STATUS_TYPE = 4,
    EQUORUS_STATUS_VERSION = 5,
    EQUORUS_STATUS_FLOAT32 = 6,
    EQUORUS_STATUS_UINT64_RANGE = 7,
    EQUORUS_STATUS_TIMESTAMP = 8,
    EQUORUS_STATUS_METRIC_UNIT = 9,
    EQUORUS_STATUS_METRIC_VALUE = 10,
    EQUORUS_STATUS_OPTION_KEYS = 11,
    EQUORUS_STATUS_DUPLICATE_KEY = 12,
    EQUORUS_STATUS_NUMBER = 13,
    EQUORUS_STATUS_UNICODE = 14,
    EQUORUS_STATUS_MALFORMED = 15,
    EQUORUS_STATUS_OUT_OF_MEMORY = 16,
    EQUORUS_STATUS_BUFFER_TOO_SMALL = 17,
    EQUORUS_STATUS_INCOMPATIBLE_ABI = 18,
    EQUORUS_STATUS_INTERNAL_ERROR = 99
} equorus_status;

typedef enum equorus_codec_profile {
    EQUORUS_CODEC_JSON_COMPACT = 0,
    EQUORUS_CODEC_CANONICAL_V1 = 1
} equorus_codec_profile;

typedef struct equorus_limits {
    uint32_t struct_size;
    uint32_t abi_version;
    uint64_t max_bytes;
    uint32_t max_depth;
    uint32_t max_items;
    uint32_t max_string_length;
} equorus_limits;

#define EQUORUS_LIMITS_MIN_SIZE \
    ((uint32_t)(offsetof(equorus_limits, max_string_length) + sizeof(uint32_t)))

/* Opaque handles */
typedef struct equorus_envelope equorus_envelope;
typedef struct equorus_buffer equorus_buffer;

/* Core decode & encode */
equorus_status equorus_decode(
    const uint8_t* bytes,
    size_t len,
    const char* expected_type,
    size_t expected_type_len,
    const equorus_limits* limits,
    equorus_envelope** out_envelope
);

equorus_status equorus_encode(
    const equorus_envelope* envelope,
    equorus_codec_profile profile,
    const equorus_limits* limits,
    equorus_buffer** out_buffer
);

/* Buffer access & release */
const uint8_t* equorus_buffer_data(const equorus_buffer* buffer);
size_t equorus_buffer_size(const equorus_buffer* buffer);
void equorus_buffer_free(equorus_buffer* buffer);

/* Envelope inspection & release */
void equorus_envelope_free(equorus_envelope* envelope);

equorus_status equorus_envelope_get_type_id(
    const equorus_envelope* envelope,
    char* out_buf,
    size_t out_buf_cap,
    size_t* out_len
);

equorus_status equorus_envelope_get_schema_version(
    const equorus_envelope* envelope,
    char* out_buf,
    size_t out_buf_cap,
    size_t* out_len
);

/* Detached canonical integrity calculation */
equorus_status equorus_calculate_integrity(
    const equorus_envelope* envelope,
    uint8_t out_sha256[32],
    equorus_buffer** out_canonical_buffer
);

/* === VINOX C ADAPTER BOUNDARY === */

#ifndef VINOX_VINOX_H
typedef enum vinox_provenance_kind {
    VINOX_PROVENANCE_SOURCE_LITERAL   = 0,
    VINOX_PROVENANCE_TOOL_EVIDENCE   = 1,
    VINOX_PROVENANCE_MODEL_GENERATED = 2,
    VINOX_PROVENANCE_DERIVED_CONTEXT = 3
} vinox_provenance_kind;

typedef struct vinox_provenance_meta {
    uint32_t struct_size;
    uint32_t kind;             /* vinox_provenance_kind */
    const char* source_id;     /* Optional source identifier */
    uint64_t timestamp_ms;     /* Timestamp in UTC milliseconds */
} vinox_provenance_meta;

#define VINOX_PROVENANCE_META_MIN_SIZE \
    ((uint32_t)(offsetof(vinox_provenance_meta, kind) + sizeof(uint32_t)))
#endif /* VINOX_VINOX_H */

equorus_status equorus_vinox_provenance_to_envelope(
    const vinox_provenance_meta* meta,
    const equorus_limits* limits,
    equorus_envelope** out_envelope
);

equorus_status equorus_vinox_provenance_from_envelope(
    const equorus_envelope* envelope,
    vinox_provenance_meta* out_meta,
    uint32_t out_struct_size,
    char* source_id_buf,
    size_t source_id_buf_cap,
    size_t* out_source_id_len
);

#ifdef __cplusplus
}
#endif

#endif /* EQUORUS_C_H */
