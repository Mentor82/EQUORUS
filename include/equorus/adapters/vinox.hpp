#ifndef EQUORUS_ADAPTERS_VINOX_HPP
#define EQUORUS_ADAPTERS_VINOX_HPP

#include "equorus/equorus_c.h"

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "equorus/envelope.hpp"
#include "equorus/pilot.hpp"

namespace equorus::adapters::vinox {

struct ProvenanceResult {
    vinox_provenance_kind kind{VINOX_PROVENANCE_SOURCE_LITERAL};
    bool has_source_id{false};
    bool is_source_id_null{false};
    std::string source_id;
    bool has_timestamp_ms{false};
    uint64_t timestamp_ms{0};

    vinox_provenance_meta to_c_meta(uint32_t struct_size = sizeof(vinox_provenance_meta)) const {
        vinox_provenance_meta meta{};
        meta.struct_size = struct_size;
        meta.kind = static_cast<uint32_t>(kind);
        if (struct_size >= offsetof(vinox_provenance_meta, source_id) + sizeof(const char*)) {
            if (has_source_id && !is_source_id_null) {
                meta.source_id = source_id.c_str();
            } else {
                meta.source_id = nullptr;
            }
        }
        if (struct_size >= offsetof(vinox_provenance_meta, timestamp_ms) + sizeof(uint64_t)) {
            meta.timestamp_ms = has_timestamp_ms ? timestamp_ms : 0;
        }
        return meta;
    }
};

Envelope to_envelope(const vinox_provenance_meta& meta, const Limits& limits = {});

ProvenanceResult from_envelope(const Envelope& env);

void from_envelope(
    const Envelope& env,
    vinox_provenance_meta* out,
    uint32_t out_struct_size,
    std::string* out_source_id_storage = nullptr
);

} // namespace equorus::adapters::vinox
#endif /* __cplusplus */

#endif /* EQUORUS_ADAPTERS_VINOX_HPP */
