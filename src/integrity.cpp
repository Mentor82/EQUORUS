#include "equorus/integrity.hpp"
#include <picosha2/picosha2.h>
#include <algorithm>
#include <bit>
#include <climits>
#include <limits>

namespace equorus {
namespace {
static_assert(CHAR_BIT == 8 && sizeof(double) == 8 &&
              std::numeric_limits<double>::is_iec559 &&
              std::numeric_limits<double>::digits == 53);
static_assert(sizeof(std::size_t) <= sizeof(std::uint64_t));
void supported(std::string_view profile, std::string_view algorithm) {
    if (profile != canonical_profile) fail(ErrorCode::profile);
    if (algorithm != integrity_algorithm) fail(ErrorCode::algorithm);
}
void validate_record(const IntegrityRecord& record) {
    supported(record.profile, record.algorithm);
    if (record.digest.size() != 64 ||
        !std::all_of(record.digest.begin(), record.digest.end(), [](unsigned char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        })) fail(ErrorCode::integrity);
}
class CanonicalWriter {
    const Limits& limits;
    std::string out;
    void append(std::string_view bytes) {
        if (bytes.size() > limits.max_bytes - out.size()) fail(ErrorCode::limit);
        out.append(bytes);
    }
    void tag(char c) { append({&c, 1}); }
    void u64(std::uint64_t n) {
        char bytes[8];
        for (int i = 7; i >= 0; --i) {
            bytes[i] = static_cast<char>(n & 255);
            n >>= 8;
        }
        append({bytes, 8});
    }
    void string(std::string_view text) {
        tag('s'); u64(text.size()); append(text);
    }
    void value(const Value& v) {
        if (std::holds_alternative<std::nullptr_t>(v.data)) tag('n');
        else if (const auto boolean = std::get_if<bool>(&v.data)) tag(*boolean ? 't' : 'f');
        else if (const auto number = std::get_if<double>(&v.data)) {
            tag('d'); u64(std::bit_cast<std::uint64_t>(*number));
        } else if (const auto text = std::get_if<std::string>(&v.data)) string(*text);
        else if (const auto array = std::get_if<Value::Array>(&v.data)) {
            tag('a'); u64(array->size());
            for (const auto& item : *array) value(item);
        } else {
            const auto& object = std::get<Value::Object>(v.data);
            tag('o'); u64(object.size());
            // Explicit unsigned UTF-8 ordering, independent of char signedness.
            std::vector<const Value::Object::value_type*> entries;
            entries.reserve(object.size());
            for (const auto& entry : object) entries.push_back(&entry);
            std::sort(entries.begin(), entries.end(), [](const auto* a, const auto* b) {
                return std::lexicographical_compare(a->first.begin(), a->first.end(),
                    b->first.begin(), b->first.end(), [](unsigned char x, unsigned char y) { return x < y; });
            });
            for (const auto* entry : entries) { string(entry->first); value(entry->second); }
        }
    }
public:
    explicit CanonicalWriter(const Limits& l) : limits(l) {}
    std::string run(const Value& v) { value(v); return std::move(out); }
};
}
std::string canonical_bytes(const Value& value, std::string_view profile, const Limits& limits) {
    if (profile != canonical_profile) fail(ErrorCode::profile);
    validate_value(value, limits);
    return CanonicalWriter(limits).run(value);
}
IntegrityRecord compute_integrity(const Envelope& envelope, std::string_view profile,
                                  std::string_view algorithm, const Limits& limits) {
    supported(profile, algorithm);
    const auto bytes = canonical_bytes(envelope.value(), profile, limits);
    picosha2::hash256_one_by_one hash;
    constexpr char domain[] = "EQUORUS-INTEGRITY\0v1\0";
    hash.process(domain, domain + sizeof(domain) - 1);
    hash.process(profile.begin(), profile.end());
    const char separator = 0;
    hash.process(&separator, &separator + 1);
    hash.process(algorithm.begin(), algorithm.end());
    hash.process(&separator, &separator + 1);
    hash.process(bytes.begin(), bytes.end());
    hash.finish();
    return {std::string(profile), std::string(algorithm), picosha2::get_hash_hex_string(hash)};
}
bool verify_integrity(const Envelope& envelope, const IntegrityRecord& record, const Limits& limits) {
    validate_record(record);
    return compute_integrity(envelope, record.profile, record.algorithm, limits).digest == record.digest;
}
std::string encode_integrity(const IntegrityRecord& record, const Limits& limits) {
    validate_record(record);
    return JsonCodec{}.encode(Value(Value::Object{
        {"canonical_profile", Value(record.profile)}, {"algorithm", Value(record.algorithm)},
        {"digest", Value(record.digest)}
    }), limits);
}
IntegrityRecord decode_integrity(std::string_view json, const Limits& limits) {
    const auto value = JsonCodec{}.decode(json, limits);
    const auto* object = std::get_if<Value::Object>(&value.data);
    if (!object || object->size() != 3) fail(ErrorCode::integrity);
    auto field = [&](std::string_view key) -> std::string {
        const auto it = object->find(key);
        if (it == object->end() || !std::holds_alternative<std::string>(it->second.data))
            fail(ErrorCode::integrity);
        return std::get<std::string>(it->second.data);
    };
    IntegrityRecord record{field("canonical_profile"), field("algorithm"), field("digest")};
    validate_record(record);
    return record;
}
} // namespace equorus
