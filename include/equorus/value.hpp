#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <utility>
#include <vector>

namespace equorus {
enum class ErrorCode {
    malformed, duplicate_key, unicode, number, limit, type, version, schema,
    uint64_range, float32, timestamp, metric_unit, metric_value, option_keys,
    profile, algorithm, integrity
};
std::string_view code_name(ErrorCode code) noexcept;
class Error : public std::runtime_error {
public:
    explicit Error(ErrorCode code) : std::runtime_error(std::string(code_name(code))), code_(code) {}
    ErrorCode code() const noexcept { return code_; }
private:
    ErrorCode code_;
};
[[noreturn]] inline void fail(ErrorCode code) { throw Error(code); }

// All strings/containers own their memory. Copies are deep value copies.
// Numeric values in this pilot are binary64, with safe-integer restrictions.
struct Value {
    using Array = std::vector<Value>;
    using Object = std::map<std::string, Value, std::less<>>;
    using Data = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;
    Data data{nullptr};
    Value() = default;
    explicit Value(std::nullptr_t) : data(nullptr) {}
    explicit Value(bool v) : data(v) {}
    explicit Value(double v) : data(v) {}
    explicit Value(std::string v) : data(std::move(v)) {}
    explicit Value(const char* v) : data(std::string(v)) {}
    explicit Value(Array v) : data(std::move(v)) {}
    explicit Value(Object v) : data(std::move(v)) {}
    bool operator==(const Value&) const = default;
};
struct Limits {
    std::size_t max_bytes{65536};
    std::size_t max_depth{12};
    std::size_t max_items{2048};
    std::size_t max_string_length{8192}; // decoded UTF-8 bytes; includes keys
};
inline constexpr std::size_t implementation_max_depth = 128;
void check_limits(const Limits&);
void validate_value(const Value&, const Limits& = {});
std::size_t unicode_length(std::string_view); // rejects invalid UTF-8
void validate_number(double);
std::uint64_t parse_uint64(std::string_view, bool nonzero = false);
} // namespace equorus
