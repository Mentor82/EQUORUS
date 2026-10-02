#include "equorus/equorus_c.h"
#include "equorus/adapters/vinox.hpp"
#include "equorus/integrity.hpp"
#include "equorus/pilot.hpp"

#include <cstring>
#include <new>
#include <vector>

struct equorus_envelope {
    equorus::Envelope env;
};

struct equorus_buffer {
    std::vector<uint8_t> data;
};

namespace {

equorus_status map_error_code(equorus::ErrorCode code) noexcept {
    switch (code) {
        case equorus::ErrorCode::malformed:     return EQUORUS_STATUS_MALFORMED;
        case equorus::ErrorCode::duplicate_key: return EQUORUS_STATUS_DUPLICATE_KEY;
        case equorus::ErrorCode::unicode:       return EQUORUS_STATUS_UNICODE;
        case equorus::ErrorCode::number:        return EQUORUS_STATUS_NUMBER;
        case equorus::ErrorCode::limit:         return EQUORUS_STATUS_LIMIT;
        case equorus::ErrorCode::type:          return EQUORUS_STATUS_TYPE;
        case equorus::ErrorCode::version:       return EQUORUS_STATUS_VERSION;
        case equorus::ErrorCode::schema:        return EQUORUS_STATUS_SCHEMA;
        case equorus::ErrorCode::uint64_range:  return EQUORUS_STATUS_UINT64_RANGE;
        case equorus::ErrorCode::float32:       return EQUORUS_STATUS_FLOAT32;
        case equorus::ErrorCode::timestamp:     return EQUORUS_STATUS_TIMESTAMP;
        case equorus::ErrorCode::metric_unit:   return EQUORUS_STATUS_METRIC_UNIT;
        case equorus::ErrorCode::metric_value:  return EQUORUS_STATUS_METRIC_VALUE;
        case equorus::ErrorCode::option_keys:   return EQUORUS_STATUS_OPTION_KEYS;
        case equorus::ErrorCode::profile:       return EQUORUS_STATUS_INVALID_ARGUMENT;
        case equorus::ErrorCode::algorithm:     return EQUORUS_STATUS_INVALID_ARGUMENT;
        case equorus::ErrorCode::integrity:     return EQUORUS_STATUS_SCHEMA;
        default: return EQUORUS_STATUS_INTERNAL_ERROR;
    }
}

equorus_status convert_limits(const equorus_limits* in, equorus::Limits& out) noexcept {
    if (in == nullptr) {
        out = equorus::Limits{};
        return EQUORUS_STATUS_OK;
    }
    if (in->struct_size < EQUORUS_LIMITS_MIN_SIZE) {
        return EQUORUS_STATUS_INVALID_ARGUMENT;
    }
    if (in->abi_version != EQUORUS_C_ABI_VERSION) {
        return EQUORUS_STATUS_INCOMPATIBLE_ABI;
    }
    out.max_bytes = in->max_bytes;
    out.max_depth = in->max_depth;
    out.max_items = in->max_items;
    out.max_string_length = in->max_string_length;
    return EQUORUS_STATUS_OK;
}

} // namespace

extern "C" {

equorus_status equorus_decode(
    const uint8_t* bytes,
    size_t len,
    const char* expected_type,
    size_t expected_type_len,
    const equorus_limits* limits,
    equorus_envelope** out_envelope
) {
    if (out_envelope == nullptr) {
        return EQUORUS_STATUS_INVALID_ARGUMENT;
    }
    *out_envelope = nullptr;

    if (bytes == nullptr && len > 0) {
        return EQUORUS_STATUS_INVALID_ARGUMENT;
    }
    if (expected_type == nullptr && expected_type_len > 0) {
        return EQUORUS_STATUS_INVALID_ARGUMENT;
    }

    equorus::Limits cpp_limits;
    equorus_status lim_status = convert_limits(limits, cpp_limits);
    if (lim_status != EQUORUS_STATUS_OK) {
        return lim_status;
    }

    try {
        std::string_view raw(reinterpret_cast<const char*>(bytes), len);
        std::string_view exp(expected_type ? expected_type : "", expected_type_len);

        auto env = equorus::pilot::decode(raw, exp, cpp_limits);
        auto* handle = new (std::nothrow) equorus_envelope{std::move(env)};
        if (handle == nullptr) {
            return EQUORUS_STATUS_OUT_OF_MEMORY;
        }
        *out_envelope = handle;
        return EQUORUS_STATUS_OK;
    } catch (const equorus::Error& e) {
        return map_error_code(e.code());
    } catch (const std::bad_alloc&) {
        return EQUORUS_STATUS_OUT_OF_MEMORY;
    } catch (...) {
        return EQUORUS_STATUS_INTERNAL_ERROR;
    }
}

equorus_status equorus_encode(
    const equorus_envelope* envelope,
    equorus_codec_profile profile,
    const equorus_limits* limits,
    equorus_buffer** out_buffer
) {
    if (out_buffer == nullptr) {
        return EQUORUS_STATUS_INVALID_ARGUMENT;
    }
    *out_buffer = nullptr;

    if (envelope == nullptr) {
        return EQUORUS_STATUS_INVALID_ARGUMENT;
    }

    equorus::Limits cpp_limits;
    equorus_status lim_status = convert_limits(limits, cpp_limits);
    if (lim_status != EQUORUS_STATUS_OK) {
        return lim_status;
    }

    try {
        std::string encoded_str;
        if (profile == EQUORUS_CODEC_JSON_COMPACT) {
            equorus::JsonCodec codec;
            encoded_str = envelope->env.encode(codec, cpp_limits);
        } else if (profile == EQUORUS_CODEC_CANONICAL_V1) {
            encoded_str = equorus::canonical_bytes(envelope->env.value(), equorus::canonical_profile, cpp_limits);
        } else {
            return EQUORUS_STATUS_INVALID_ARGUMENT;
        }

        auto* buf = new (std::nothrow) equorus_buffer;
        if (buf == nullptr) {
            return EQUORUS_STATUS_OUT_OF_MEMORY;
        }
        buf->data.assign(encoded_str.begin(), encoded_str.end());
        *out_buffer = buf;
        return EQUORUS_STATUS_OK;
    } catch (const equorus::Error& e) {
        return map_error_code(e.code());
    } catch (const std::bad_alloc&) {
        return EQUORUS_STATUS_OUT_OF_MEMORY;
    } catch (...) {
        return EQUORUS_STATUS_INTERNAL_ERROR;
    }
}

const uint8_t* equorus_buffer_data(const equorus_buffer* buffer) {
    if (buffer == nullptr || buffer->data.empty()) {
        return nullptr;
    }
    return buffer->data.data();
}

size_t equorus_buffer_size(const equorus_buffer* buffer) {
    if (buffer == nullptr) {
        return 0;
    }
    return buffer->data.size();
}

void equorus_buffer_free(equorus_buffer* buffer) {
    delete buffer;
}

void equorus_envelope_free(equorus_envelope* envelope) {
    delete envelope;
}

equorus_status equorus_envelope_get_type_id(
    const equorus_envelope* envelope,
    char* out_buf,
    size_t out_buf_cap,
    size_t* out_len
) {
    if (envelope == nullptr || out_len == nullptr) {
        return EQUORUS_STATUS_INVALID_ARGUMENT;
    }

    std::string_view tid = envelope->env.type_id();
    *out_len = tid.size();

    if (out_buf == nullptr || out_buf_cap <= tid.size()) {
        return EQUORUS_STATUS_BUFFER_TOO_SMALL;
    }

    std::memcpy(out_buf, tid.data(), tid.size());
    out_buf[tid.size()] = '\0';
    return EQUORUS_STATUS_OK;
}

equorus_status equorus_envelope_get_schema_version(
    const equorus_envelope* envelope,
    char* out_buf,
    size_t out_buf_cap,
    size_t* out_len
) {
    if (envelope == nullptr || out_len == nullptr) {
        return EQUORUS_STATUS_INVALID_ARGUMENT;
    }

    std::string_view ver = envelope->env.schema_version();
    *out_len = ver.size();

    if (out_buf == nullptr || out_buf_cap <= ver.size()) {
        return EQUORUS_STATUS_BUFFER_TOO_SMALL;
    }

    std::memcpy(out_buf, ver.data(), ver.size());
    out_buf[ver.size()] = '\0';
    return EQUORUS_STATUS_OK;
}

equorus_status equorus_calculate_integrity(
    const equorus_envelope* envelope,
    uint8_t out_sha256[32],
    equorus_buffer** out_canonical_buffer
) {
    if (envelope == nullptr || out_sha256 == nullptr) {
        return EQUORUS_STATUS_INVALID_ARGUMENT;
    }
    if (out_canonical_buffer != nullptr) {
        *out_canonical_buffer = nullptr;
    }

    try {
        equorus::Limits limits;
        auto rec = equorus::compute_integrity(envelope->env, equorus::canonical_profile, equorus::integrity_algorithm, limits);
        
        // Parse 64 hex characters into 32 bytes
        if (rec.digest.size() != 64) {
            return EQUORUS_STATUS_INTERNAL_ERROR;
        }
        for (size_t i = 0; i < 32; ++i) {
            char hex_byte[3] = {rec.digest[i * 2], rec.digest[i * 2 + 1], '\0'};
            out_sha256[i] = static_cast<uint8_t>(std::strtoul(hex_byte, nullptr, 16));
        }

        if (out_canonical_buffer != nullptr) {
            std::string canon = equorus::canonical_bytes(envelope->env.value(), equorus::canonical_profile, limits);
            auto* buf = new (std::nothrow) equorus_buffer;
            if (buf == nullptr) {
                return EQUORUS_STATUS_OUT_OF_MEMORY;
            }
            buf->data.assign(canon.begin(), canon.end());
            *out_canonical_buffer = buf;
        }

        return EQUORUS_STATUS_OK;
    } catch (const equorus::Error& e) {
        return map_error_code(e.code());
    } catch (const std::bad_alloc&) {
        return EQUORUS_STATUS_OUT_OF_MEMORY;
    } catch (...) {
        return EQUORUS_STATUS_INTERNAL_ERROR;
    }
}

equorus_status equorus_vinox_provenance_to_envelope(
    const vinox_provenance_meta* meta,
    const equorus_limits* limits,
    equorus_envelope** out_envelope
) {
    if (out_envelope == nullptr) {
        return EQUORUS_STATUS_INVALID_ARGUMENT;
    }
    *out_envelope = nullptr;

    if (meta == nullptr) {
        return EQUORUS_STATUS_INVALID_ARGUMENT;
    }
    if (meta->struct_size < VINOX_PROVENANCE_META_MIN_SIZE) {
        return EQUORUS_STATUS_LIMIT;
    }
    if (meta->kind > 3) {
        return EQUORUS_STATUS_SCHEMA;
    }

    equorus::Limits cpp_limits;
    equorus_status lim_status = convert_limits(limits, cpp_limits);
    if (lim_status != EQUORUS_STATUS_OK) {
        return lim_status;
    }

    try {
        auto env = equorus::adapters::vinox::to_envelope(*meta, cpp_limits);
        auto* handle = new (std::nothrow) equorus_envelope{std::move(env)};
        if (handle == nullptr) {
            return EQUORUS_STATUS_OUT_OF_MEMORY;
        }
        *out_envelope = handle;
        return EQUORUS_STATUS_OK;
    } catch (const equorus::Error& e) {
        return map_error_code(e.code());
    } catch (const std::bad_alloc&) {
        return EQUORUS_STATUS_OUT_OF_MEMORY;
    } catch (...) {
        return EQUORUS_STATUS_INTERNAL_ERROR;
    }
}

equorus_status equorus_vinox_provenance_from_envelope(
    const equorus_envelope* envelope,
    vinox_provenance_meta* out_meta,
    uint32_t out_struct_size,
    char* source_id_buf,
    size_t source_id_buf_cap,
    size_t* out_source_id_len
) {
    if (envelope == nullptr || out_meta == nullptr) {
        return EQUORUS_STATUS_INVALID_ARGUMENT;
    }
    if (out_struct_size < VINOX_PROVENANCE_META_MIN_SIZE) {
        return EQUORUS_STATUS_LIMIT;
    }

    try {
        auto res = equorus::adapters::vinox::from_envelope(envelope->env);

        if (res.has_source_id && out_struct_size < offsetof(vinox_provenance_meta, source_id) + sizeof(const char*)) {
            return EQUORUS_STATUS_LIMIT;
        }
        if (res.has_timestamp_ms && out_struct_size < offsetof(vinox_provenance_meta, timestamp_ms) + sizeof(uint64_t)) {
            return EQUORUS_STATUS_LIMIT;
        }

        out_meta->kind = static_cast<uint32_t>(res.kind);

        if (out_struct_size >= offsetof(vinox_provenance_meta, source_id) + sizeof(const char*)) {
            if (res.has_source_id && !res.is_source_id_null) {
                size_t needed = res.source_id.size();
                if (out_source_id_len != nullptr) {
                    *out_source_id_len = needed;
                }
                if (source_id_buf == nullptr || source_id_buf_cap <= needed) {
                    return EQUORUS_STATUS_BUFFER_TOO_SMALL;
                }
                std::memcpy(source_id_buf, res.source_id.c_str(), needed);
                source_id_buf[needed] = '\0';
                out_meta->source_id = source_id_buf;
            } else {
                if (out_source_id_len != nullptr) {
                    *out_source_id_len = 0;
                }
                out_meta->source_id = nullptr;
            }
        }

        if (out_struct_size >= offsetof(vinox_provenance_meta, timestamp_ms) + sizeof(uint64_t)) {
            if (res.has_timestamp_ms) {
                out_meta->timestamp_ms = res.timestamp_ms;
                out_meta->struct_size = out_struct_size;
            } else {
                out_meta->timestamp_ms = 0;
                out_meta->struct_size = static_cast<uint32_t>(offsetof(vinox_provenance_meta, timestamp_ms));
            }
        } else {
            out_meta->struct_size = out_struct_size;
        }

        if (!res.has_source_id && !res.has_timestamp_ms) {
            out_meta->struct_size = VINOX_PROVENANCE_META_MIN_SIZE;
        }

        return EQUORUS_STATUS_OK;
    } catch (const equorus::Error& e) {
        return map_error_code(e.code());
    } catch (const std::bad_alloc&) {
        return EQUORUS_STATUS_OUT_OF_MEMORY;
    } catch (...) {
        return EQUORUS_STATUS_INTERNAL_ERROR;
    }
}

} // extern "C"
