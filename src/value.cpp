#include "equorus/value.hpp"
#include <charconv>
#include <cmath>
#include <algorithm>
namespace equorus {
std::string_view code_name(ErrorCode code) noexcept {
    switch (code) {
#define E(x,y) case ErrorCode::x: return y;
    E(malformed,"MALFORMED") E(duplicate_key,"DUPLICATE_KEY") E(unicode,"UNICODE")
    E(number,"NUMBER") E(limit,"LIMIT") E(type,"TYPE") E(version,"VERSION")
    E(schema,"SCHEMA") E(uint64_range,"UINT64_RANGE") E(float32,"FLOAT32")
    E(timestamp,"TIMESTAMP") E(metric_unit,"METRIC_UNIT") E(metric_value,"METRIC_VALUE")
    E(option_keys,"OPTION_KEYS")
    E(profile,"PROFILE") E(algorithm,"ALGORITHM") E(integrity,"INTEGRITY")
#undef E
    }
    return "INTERNAL";
}
void check_limits(const Limits& l) {
    if (l.max_depth > implementation_max_depth) fail(ErrorCode::limit);
}
std::size_t unicode_length(std::string_view s) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < s.size(); ++count) {
        const auto c = static_cast<unsigned char>(s[i++]);
        if (c < 0x80) continue;
        unsigned n = 0;
        std::uint32_t cp = 0, minimum = 0;
        if (c >= 0xC2 && c <= 0xDF) { n = 1; cp = c & 31; minimum = 0x80; }
        else if (c >= 0xE0 && c <= 0xEF) { n = 2; cp = c & 15; minimum = 0x800; }
        else if (c >= 0xF0 && c <= 0xF4) { n = 3; cp = c & 7; minimum = 0x10000; }
        else fail(ErrorCode::unicode);
        if (n > s.size() - i) fail(ErrorCode::unicode);
        while (n--) {
            const auto b = static_cast<unsigned char>(s[i++]);
            if ((b & 0xC0) != 0x80) fail(ErrorCode::unicode);
            cp = (cp << 6) | (b & 63);
        }
        if (cp < minimum || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            fail(ErrorCode::unicode);
    }
    return count;
}
void validate_number(double v) {
    if (!std::isfinite(v) || (v == 0 && std::signbit(v)) ||
        (std::trunc(v) == v && std::abs(v) > 9007199254740991.0)) fail(ErrorCode::number);
}
namespace {
void walk(const Value& v, const Limits& l, std::size_t depth, std::size_t& count) {
    if (depth > l.max_depth || count >= l.max_items) fail(ErrorCode::limit);
    ++count;
    auto string = [&](const std::string& s) {
        if (s.size() > l.max_string_length) fail(ErrorCode::limit);
        unicode_length(s);
    };
    if (auto numeric = std::get_if<double>(&v.data)) validate_number(*numeric);
    else if (auto text = std::get_if<std::string>(&v.data)) string(*text);
    else if (auto array = std::get_if<Value::Array>(&v.data))
        for (const auto& x : *array) walk(x, l, depth + 1, count);
    else if (auto object = std::get_if<Value::Object>(&v.data))
        for (const auto& [key, x] : *object) { string(key); walk(x, l, depth + 1, count); }
}
}
void validate_value(const Value& v, const Limits& l) {
    check_limits(l);
    std::size_t count = 0;
    walk(v, l, 1, count);
}
std::uint64_t parse_uint64(std::string_view text, bool nonzero) {
    if (text.empty() || text.size() > 20 || (text.size() > 1 && text.front() == '0') ||
        !std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; }))
        fail(ErrorCode::schema);
    std::uint64_t value{};
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || (nonzero && !value))
        fail(ErrorCode::uint64_range);
    return value;
}
}
