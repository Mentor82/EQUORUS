#include "equorus/equorus_c.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void check(int condition, const char* msg) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", msg);
        exit(1);
    }
}

int main(void) {
    printf("Starting C ABI smoke tests (pure C)...\n");

    /* 1. Null and invalid argument tests */
    {
        equorus_envelope* env = NULL;
        equorus_status s = equorus_decode(NULL, 10, "vinox.provenance.snapshot", 25, NULL, &env);
        check(s == EQUORUS_STATUS_INVALID_ARGUMENT, "decode null bytes with positive len");
        check(env == NULL, "env handle remains null on failure");

        s = equorus_decode((const uint8_t*)"{}", 2, NULL, 5, NULL, &env);
        check(s == EQUORUS_STATUS_INVALID_ARGUMENT, "decode null type with positive len");

        s = equorus_decode((const uint8_t*)"{}", 2, "vinox.provenance.snapshot", 25, NULL, NULL);
        check(s == EQUORUS_STATUS_INVALID_ARGUMENT, "decode null out_envelope");

        s = equorus_encode(NULL, EQUORUS_CODEC_JSON_COMPACT, NULL, NULL);
        check(s == EQUORUS_STATUS_INVALID_ARGUMENT, "encode null out_buffer");
    }

    /* 2. Options struct sizing & ABI versioning */
    {
        equorus_envelope* env = NULL;
        equorus_limits short_lim;
        memset(&short_lim, 0, sizeof(short_lim));
        short_lim.struct_size = 4; /* Less than EQUORUS_LIMITS_MIN_SIZE */
        short_lim.abi_version = EQUORUS_C_ABI_VERSION;

        const char* json = "{\"type_id\":\"vinox.provenance.snapshot\",\"schema_version\":\"0.1\",\"provenance\":{\"kind\":\"SOURCE_LITERAL\"},\"payload\":{}}";
        equorus_status s = equorus_decode((const uint8_t*)json, strlen(json), "vinox.provenance.snapshot", 25, &short_lim, &env);
        check(s == EQUORUS_STATUS_INVALID_ARGUMENT, "short limits struct rejected");

        equorus_limits bad_abi;
        memset(&bad_abi, 0, sizeof(bad_abi));
        bad_abi.struct_size = sizeof(equorus_limits);
        bad_abi.abi_version = 999; /* Unknown ABI version */
        s = equorus_decode((const uint8_t*)json, strlen(json), "vinox.provenance.snapshot", 25, &bad_abi, &env);
        check(s == EQUORUS_STATUS_INCOMPATIBLE_ABI, "bad ABI version rejected");
    }

    /* 3. Successful decode, inspection, encode, and release */
    {
        const char* json = "{\"type_id\":\"vinox.provenance.snapshot\",\"schema_version\":\"0.1\",\"provenance\":{\"kind\":\"SOURCE_LITERAL\"},\"payload\":{}}";
        equorus_envelope* env = NULL;
        equorus_status s = equorus_decode((const uint8_t*)json, strlen(json), "vinox.provenance.snapshot", 25, NULL, &env);
        check(s == EQUORUS_STATUS_OK, "decode valid envelope");
        check(env != NULL, "env handle created");

        char type_buf[64];
        size_t type_len = 0;
        /* Small buffer check */
        s = equorus_envelope_get_type_id(env, type_buf, 5, &type_len);
        check(s == EQUORUS_STATUS_BUFFER_TOO_SMALL, "type id buffer too small");
        check(type_len == strlen("vinox.provenance.snapshot"), "type len set correctly");

        /* Adequate buffer */
        s = equorus_envelope_get_type_id(env, type_buf, sizeof(type_buf), &type_len);
        check(s == EQUORUS_STATUS_OK, "type id query OK");
        check(strcmp(type_buf, "vinox.provenance.snapshot") == 0, "type id match");

        char ver_buf[16];
        size_t ver_len = 0;
        s = equorus_envelope_get_schema_version(env, ver_buf, sizeof(ver_buf), &ver_len);
        check(s == EQUORUS_STATUS_OK, "schema version query OK");
        check(strcmp(ver_buf, "0.1") == 0, "schema version match");

        /* Encode JSON compact */
        equorus_buffer* buf = NULL;
        s = equorus_encode(env, EQUORUS_CODEC_JSON_COMPACT, NULL, &buf);
        check(s == EQUORUS_STATUS_OK, "encode compact OK");
        check(buf != NULL, "buffer allocated");
        check(equorus_buffer_size(buf) > 0, "buffer has bytes");
        check(equorus_buffer_data(buf) != NULL, "buffer data non-null");

        /* Encode canonical V1 */
        equorus_buffer* canon_buf = NULL;
        s = equorus_encode(env, EQUORUS_CODEC_CANONICAL_V1, NULL, &canon_buf);
        check(s == EQUORUS_STATUS_OK, "encode canonical OK");
        check(canon_buf != NULL, "canonical buffer allocated");

        /* Calculate integrity */
        uint8_t sha[32];
        equorus_buffer* integrity_canon = NULL;
        s = equorus_calculate_integrity(env, sha, &integrity_canon);
        check(s == EQUORUS_STATUS_OK, "calculate integrity OK");
        check(integrity_canon != NULL, "integrity canonical buffer allocated");
        check(equorus_buffer_size(canon_buf) == equorus_buffer_size(integrity_canon), "canonical sizes match");
        check(memcmp(equorus_buffer_data(canon_buf), equorus_buffer_data(integrity_canon), equorus_buffer_size(canon_buf)) == 0, "canonical bytes match");

        /* Release in arbitrary order, including null releases */
        equorus_buffer_free(buf);
        equorus_envelope_free(env);
        /* Buffer outlives its envelope */
        check(equorus_buffer_size(canon_buf) > 0, "canon buf alive after envelope free");
        equorus_buffer_free(canon_buf);
        equorus_buffer_free(integrity_canon);
        equorus_buffer_free(NULL);
        equorus_envelope_free(NULL);
    }

    /* 4. VINOX C adapter roundtrips */
    {
        /* Minimal meta */
        vinox_provenance_meta min_meta;
        memset(&min_meta, 0, sizeof(min_meta));
        min_meta.struct_size = VINOX_PROVENANCE_META_MIN_SIZE;
        min_meta.kind = VINOX_PROVENANCE_SOURCE_LITERAL;

        equorus_envelope* env = NULL;
        equorus_status s = equorus_vinox_provenance_to_envelope(&min_meta, NULL, &env);
        check(s == EQUORUS_STATUS_OK, "vinox min meta to envelope");
        check(env != NULL, "env created");

        vinox_provenance_meta back_meta;
        memset(&back_meta, 0, sizeof(back_meta));
        char src_buf[64];
        size_t src_len = 0;
        s = equorus_vinox_provenance_from_envelope(env, &back_meta, sizeof(back_meta), src_buf, sizeof(src_buf), &src_len);
        check(s == EQUORUS_STATUS_OK, "envelope to vinox min meta");
        check(back_meta.kind == VINOX_PROVENANCE_SOURCE_LITERAL, "kind match");
        check(back_meta.source_id == NULL, "source_id is null for min meta");
        check(back_meta.struct_size == VINOX_PROVENANCE_META_MIN_SIZE, "struct_size reflects min meta");
        equorus_envelope_free(env);

        /* Full meta with source_id and timestamp_ms */
        vinox_provenance_meta full_meta;
        memset(&full_meta, 0, sizeof(full_meta));
        full_meta.struct_size = sizeof(vinox_provenance_meta);
        full_meta.kind = VINOX_PROVENANCE_TOOL_EVIDENCE;
        full_meta.source_id = "fixture:vinox/tool/test";
        full_meta.timestamp_ms = 18446744073709551615ULL;

        s = equorus_vinox_provenance_to_envelope(&full_meta, NULL, &env);
        check(s == EQUORUS_STATUS_OK, "vinox full meta to envelope");

        /* Small source_id buffer */
        s = equorus_vinox_provenance_from_envelope(env, &back_meta, sizeof(back_meta), src_buf, 5, &src_len);
        check(s == EQUORUS_STATUS_BUFFER_TOO_SMALL, "small source_id buffer rejected");
        check(src_len == strlen("fixture:vinox/tool/test"), "needed length reported");

        /* Proper buffer */
        s = equorus_vinox_provenance_from_envelope(env, &back_meta, sizeof(back_meta), src_buf, sizeof(src_buf), &src_len);
        check(s == EQUORUS_STATUS_OK, "full meta reverse conversion OK");
        check(back_meta.kind == VINOX_PROVENANCE_TOOL_EVIDENCE, "full kind match");
        check(back_meta.source_id != NULL && strcmp(back_meta.source_id, "fixture:vinox/tool/test") == 0, "source_id match");
        check(back_meta.timestamp_ms == 18446744073709551615ULL, "timestamp_ms match");
        check(back_meta.struct_size == sizeof(vinox_provenance_meta), "full struct_size set");

        equorus_envelope_free(env);

        /* Invalid kind rejection */
        full_meta.kind = 99;
        s = equorus_vinox_provenance_to_envelope(&full_meta, NULL, &env);
        check(s == EQUORUS_STATUS_SCHEMA, "invalid kind rejected");
        check(env == NULL, "env null on invalid kind");

        /* Short struct rejection */
        full_meta.kind = VINOX_PROVENANCE_SOURCE_LITERAL;
        full_meta.struct_size = 4;
        s = equorus_vinox_provenance_to_envelope(&full_meta, NULL, &env);
        check(s == EQUORUS_STATUS_LIMIT, "short struct rejected");
    }

    printf("All C ABI smoke tests passed successfully.\n");
    return 0;
}
