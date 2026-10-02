#include "equorus/adapters/vinox.hpp"
#include "equorus/adapters/linep.hpp"
#include <cstring>

namespace equorus::adapters::vinox {

Envelope to_envelope(const vinox_provenance_meta& meta, const Limits& limits) {
    if (meta.struct_size < VINOX_PROVENANCE_META_MIN_SIZE) {
        fail(ErrorCode::limit);
    }
    if (meta.kind > 3) {
        fail(ErrorCode::schema);
    }

    std::string_view kind_name;
    switch (meta.kind) {
        case VINOX_PROVENANCE_SOURCE_LITERAL:   kind_name = "SOURCE_LITERAL"; break;
        case VINOX_PROVENANCE_TOOL_EVIDENCE:   kind_name = "TOOL_EVIDENCE"; break;
        case VINOX_PROVENANCE_MODEL_GENERATED: kind_name = "MODEL_GENERATED"; break;
        case VINOX_PROVENANCE_DERIVED_CONTEXT: kind_name = "DERIVED_CONTEXT"; break;
        default: fail(ErrorCode::schema);
    }

    Value::Object prov{{"kind", Value(std::string(kind_name))}};

    if (meta.struct_size >= offsetof(vinox_provenance_meta, source_id) + sizeof(const char*)) {
        if (meta.source_id == nullptr) {
            prov["source_id"] = Value(nullptr);
        } else {
            prov["source_id"] = Value(std::string(meta.source_id));
        }
    }

    if (meta.struct_size >= offsetof(vinox_provenance_meta, timestamp_ms) + sizeof(uint64_t)) {
        prov["timestamp_ms"] = Value(std::to_string(meta.timestamp_ms));
    }

    Value root(Value::Object{
        {"type_id", Value("vinox.provenance.snapshot")},
        {"schema_version", Value("0.1")},
        {"provenance", Value(std::move(prov))},
        {"payload", Value(Value::Object{})}
    });

    return pilot::create(root, "vinox.provenance.snapshot", limits);
}

ProvenanceResult from_envelope(const Envelope& env) {
    if (env.type_id() != "vinox.provenance.snapshot") {
        fail(ErrorCode::type);
    }

    const auto& root_obj = std::get<Value::Object>(env.value().data);
    const auto& prov_obj = std::get<Value::Object>(root_obj.at("provenance").data);

    ProvenanceResult res{};
    const auto& kind_str = std::get<std::string>(prov_obj.at("kind").data);
    if (kind_str == "SOURCE_LITERAL") {
        res.kind = VINOX_PROVENANCE_SOURCE_LITERAL;
    } else if (kind_str == "TOOL_EVIDENCE") {
        res.kind = VINOX_PROVENANCE_TOOL_EVIDENCE;
    } else if (kind_str == "MODEL_GENERATED") {
        res.kind = VINOX_PROVENANCE_MODEL_GENERATED;
    } else if (kind_str == "DERIVED_CONTEXT") {
        res.kind = VINOX_PROVENANCE_DERIVED_CONTEXT;
    } else {
        fail(ErrorCode::schema);
    }

    auto src_it = prov_obj.find("source_id");
    if (src_it != prov_obj.end()) {
        res.has_source_id = true;
        if (std::holds_alternative<std::nullptr_t>(src_it->second.data)) {
            res.is_source_id_null = true;
        } else if (std::holds_alternative<std::string>(src_it->second.data)) {
            res.is_source_id_null = false;
            res.source_id = std::get<std::string>(src_it->second.data);
        } else {
            fail(ErrorCode::schema);
        }
    }

    auto ts_it = prov_obj.find("timestamp_ms");
    if (ts_it != prov_obj.end()) {
        res.has_timestamp_ms = true;
        const auto& ts_str = std::get<std::string>(ts_it->second.data);
        res.timestamp_ms = parse_uint64(ts_str, false);
    }

    return res;
}

void from_envelope(
    const Envelope& env,
    vinox_provenance_meta* out,
    uint32_t out_struct_size,
    std::string* out_source_id_storage
) {
    if (out == nullptr) {
        fail(ErrorCode::schema);
    }
    if (out_struct_size < VINOX_PROVENANCE_META_MIN_SIZE) {
        fail(ErrorCode::limit);
    }

    ProvenanceResult res = from_envelope(env);

    if (res.has_source_id && out_struct_size < offsetof(vinox_provenance_meta, source_id) + sizeof(const char*)) {
        fail(ErrorCode::limit);
    }
    if (res.has_timestamp_ms && out_struct_size < offsetof(vinox_provenance_meta, timestamp_ms) + sizeof(uint64_t)) {
        fail(ErrorCode::limit);
    }

    out->kind = static_cast<uint32_t>(res.kind);

    if (out_struct_size >= offsetof(vinox_provenance_meta, source_id) + sizeof(const char*)) {
        if (res.has_source_id && !res.is_source_id_null) {
            if (out_source_id_storage == nullptr) {
                fail(ErrorCode::schema);
            }
            *out_source_id_storage = std::move(res.source_id);
            out->source_id = out_source_id_storage->c_str();
        } else {
            out->source_id = nullptr;
        }
    }

    if (out_struct_size >= offsetof(vinox_provenance_meta, timestamp_ms) + sizeof(uint64_t)) {
        if (res.has_timestamp_ms) {
            out->timestamp_ms = res.timestamp_ms;
            out->struct_size = out_struct_size;
        } else {
            out->timestamp_ms = 0;
            // Indicating timestamp_ms is absent to avoid synthesizing a missing timestamp
            out->struct_size = static_cast<uint32_t>(offsetof(vinox_provenance_meta, timestamp_ms));
        }
    } else {
        out->struct_size = out_struct_size;
    }

    if (!res.has_source_id && !res.has_timestamp_ms) {
        out->struct_size = VINOX_PROVENANCE_META_MIN_SIZE;
    }
}

} // namespace equorus::adapters::vinox

namespace equorus::adapters::linep {

Envelope to_envelope(
    const LinePRequestEnvelope& req,
    const Value::Object& provenance,
    const Limits& limits
) {
    if (req.profile == RuntimeProfile::unspecified) {
        fail(ErrorCode::schema);
    }
    if (req.model_id.empty()) {
        fail(ErrorCode::schema);
    }
    if (!req.stream.is_valid()) {
        fail(ErrorCode::uint64_range);
    }

    std::string_view profile_str;
    switch (req.profile) {
        case RuntimeProfile::generate: profile_str = "generate"; break;
        case RuntimeProfile::chat:     profile_str = "chat";     break;
        case RuntimeProfile::embed:    profile_str = "embed";    break;
        default: fail(ErrorCode::schema);
    }

    Value::Object stream_obj{
        {"request_id",   Value(std::to_string(req.stream.request_id))},
        {"execution_id", Value(std::to_string(req.stream.execution_id))},
        {"output_id",    Value(static_cast<double>(req.stream.output_id))}
    };

    double temp_d = static_cast<double>(req.temperature);
    if (static_cast<float>(temp_d) != req.temperature) {
        fail(ErrorCode::float32);
    }

    Value::Object payload_obj{
        {"protocol_version", Value("0.2")},
        {"stream",           Value(std::move(stream_obj))},
        {"profile",          Value(std::string(profile_str))},
        {"model_id",         Value(req.model_id)},
        {"payload",          Value(req.payload)},
        {"max_tokens",       Value(static_cast<double>(req.max_tokens))},
        {"temperature",      Value(temp_d)},
        {"stream_requested", Value(req.stream_requested)},
        {"has_options",      Value(req.has_options)}
    };

    if (req.has_options) {
        const auto& opt = req.options;
        Value::Object opt_obj{
            {"top_p",             Value(static_cast<double>(opt.top_p))},
            {"top_k",             Value(static_cast<double>(opt.top_k))},
            {"repeat_penalty",    Value(static_cast<double>(opt.repeat_penalty))},
            {"repeat_last_n",      Value(static_cast<double>(opt.repeat_last_n))},
            {"seed",              Value(std::to_string(opt.seed))},
            {"presence_penalty",  Value(static_cast<double>(opt.presence_penalty))},
            {"frequency_penalty", Value(static_cast<double>(opt.frequency_penalty))}
        };

        Value::Array stop_seqs;
        for (const auto& s : opt.stop_sequences) {
            stop_seqs.push_back(Value(s));
        }
        opt_obj["stop_sequences"] = Value(std::move(stop_seqs));

        // Sort extra_options by UTF-8 bytes of key
        auto extra_sorted = opt.extra_options;
        auto byte_less = [](const std::string& a, const std::string& b) {
            return std::lexicographical_compare(
                a.begin(), a.end(), b.begin(), b.end(),
                [](unsigned char x, unsigned char y) { return x < y; });
        };
        std::sort(extra_sorted.begin(), extra_sorted.end(), [&](const auto& a, const auto& b) {
            return byte_less(a.first, b.first);
        });

        // Ensure keys are unique
        for (size_t i = 1; i < extra_sorted.size(); ++i) {
            if (extra_sorted[i - 1].first == extra_sorted[i].first) {
                fail(ErrorCode::option_keys);
            }
        }

        Value::Array extra_arr;
        for (const auto& [k, v] : extra_sorted) {
            extra_arr.push_back(Value(Value::Array{Value(k), Value(v)}));
        }
        opt_obj["extra_options"] = Value(std::move(extra_arr));

        payload_obj["options"] = Value(std::move(opt_obj));
    }

    Value root(Value::Object{
        {"type_id",        Value("linep.v02.request")},
        {"schema_version", Value("0.1")},
        {"provenance",     Value(provenance)},
        {"payload",        Value(std::move(payload_obj))}
    });

    return pilot::create(root, "linep.v02.request", limits);
}

LinePRequestEnvelope from_envelope(const Envelope& env) {
    if (env.type_id() != "linep.v02.request") {
        fail(ErrorCode::type);
    }

    const auto& root_obj = std::get<Value::Object>(env.value().data);
    const auto& payload_obj = std::get<Value::Object>(root_obj.at("payload").data);

    LinePRequestEnvelope req{};

    const auto& stream_obj = std::get<Value::Object>(payload_obj.at("stream").data);
    req.stream.request_id = parse_uint64(std::get<std::string>(stream_obj.at("request_id").data), true);
    req.stream.execution_id = parse_uint64(std::get<std::string>(stream_obj.at("execution_id").data), true);
    req.stream.output_id = static_cast<uint32_t>(std::get<double>(stream_obj.at("output_id").data));

    const auto& prof_str = std::get<std::string>(payload_obj.at("profile").data);
    if (prof_str == "generate") {
        req.profile = RuntimeProfile::generate;
    } else if (prof_str == "chat") {
        req.profile = RuntimeProfile::chat;
    } else if (prof_str == "embed") {
        req.profile = RuntimeProfile::embed;
    } else {
        fail(ErrorCode::schema);
    }

    req.model_id = std::get<std::string>(payload_obj.at("model_id").data);
    req.payload = std::get<std::string>(payload_obj.at("payload").data);
    req.max_tokens = static_cast<uint32_t>(std::get<double>(payload_obj.at("max_tokens").data));
    req.temperature = static_cast<float>(std::get<double>(payload_obj.at("temperature").data));
    req.stream_requested = std::get<bool>(payload_obj.at("stream_requested").data);
    req.has_options = std::get<bool>(payload_obj.at("has_options").data);

    if (req.has_options) {
        const auto& opt_obj = std::get<Value::Object>(payload_obj.at("options").data);
        req.options.top_p = static_cast<float>(std::get<double>(opt_obj.at("top_p").data));
        req.options.top_k = static_cast<int32_t>(std::get<double>(opt_obj.at("top_k").data));
        req.options.repeat_penalty = static_cast<float>(std::get<double>(opt_obj.at("repeat_penalty").data));
        req.options.repeat_last_n = static_cast<int32_t>(std::get<double>(opt_obj.at("repeat_last_n").data));
        req.options.seed = parse_uint64(std::get<std::string>(opt_obj.at("seed").data), false);
        req.options.presence_penalty = static_cast<float>(std::get<double>(opt_obj.at("presence_penalty").data));
        req.options.frequency_penalty = static_cast<float>(std::get<double>(opt_obj.at("frequency_penalty").data));

        const auto& stop_arr = std::get<Value::Array>(opt_obj.at("stop_sequences").data);
        for (const auto& item : stop_arr) {
            req.options.stop_sequences.push_back(std::get<std::string>(item.data));
        }

        const auto& extra_arr = std::get<Value::Array>(opt_obj.at("extra_options").data);
        for (const auto& pair_val : extra_arr) {
            const auto& pair = std::get<Value::Array>(pair_val.data);
            req.options.extra_options.emplace_back(
                std::get<std::string>(pair[0].data),
                std::get<std::string>(pair[1].data)
            );
        }
    }

    return req;
}

} // namespace equorus::adapters::linep
