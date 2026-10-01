#include "equorus/envelope.hpp"
#include <algorithm>
namespace equorus {
namespace {
std::string_view field(const Value& root, std::string_view name, ErrorCode code) {
    const auto* o = std::get_if<Value::Object>(&root.data);
    if (!o) fail(ErrorCode::schema);
    const auto it = o->find(name);
    if (it == o->end() || !std::holds_alternative<std::string>(it->second.data)) fail(code);
    return std::get<std::string>(it->second.data);
}
}
Envelope Envelope::create(const Value& root, std::string_view expected,
                          std::span<const std::string_view> versions,
                          const Validator& validator, const Limits& limits) {
    validate_value(root, limits);
    if (expected.empty() || field(root, "type_id", ErrorCode::type) != expected) fail(ErrorCode::type);
    const auto version = field(root, "schema_version", ErrorCode::version);
    if (std::find(versions.begin(), versions.end(), version) == versions.end()) fail(ErrorCode::version);
    const auto& o = std::get<Value::Object>(root.data);
    if (o.size() != 4 || !o.contains("provenance") || !o.contains("payload") ||
        !std::holds_alternative<Value::Object>(o.at("provenance").data) || !validator)
        fail(ErrorCode::schema);
    validator(root);
    return Envelope(root);
}
Envelope Envelope::decode(const Codec& codec, std::string_view bytes, std::string_view expected,
                          std::span<const std::string_view> versions,
                          const Validator& validator, const Limits& limits) {
    return create(codec.decode(bytes, limits), expected, versions, validator, limits);
}
std::string Envelope::encode(const Codec& codec, const Limits& limits) const {
    return codec.encode(root_, limits);
}
std::string_view Envelope::type_id() const { return field(root_, "type_id", ErrorCode::type); }
std::string_view Envelope::schema_version() const { return field(root_, "schema_version", ErrorCode::version); }
}
