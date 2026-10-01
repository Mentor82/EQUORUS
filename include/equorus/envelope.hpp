#pragma once
#include "equorus/codec.hpp"
#include <functional>
#include <span>
namespace equorus {
using Validator = std::function<void(const Value&)>;
class Envelope {
public:
    static Envelope create(const Value& root, std::string_view expected_type,
                           std::span<const std::string_view> supported_versions,
                           const Validator& validator, const Limits& limits = {});
    static Envelope decode(const Codec&, std::string_view bytes, std::string_view expected_type,
                           std::span<const std::string_view> supported_versions,
                           const Validator& validator, const Limits& limits = {});
    std::string encode(const Codec& codec, const Limits& limits = {}) const;
    const Value& value() const noexcept { return root_; }
    std::string_view type_id() const;
    std::string_view schema_version() const;
private:
    explicit Envelope(Value root) : root_(std::move(root)) {}
    Value root_;
};
} // namespace equorus
