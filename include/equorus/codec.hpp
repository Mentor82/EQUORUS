#pragma once
#include "equorus/value.hpp"
namespace equorus {
class Codec {
public:
    virtual ~Codec() = default;
    virtual Value decode(std::string_view bytes, const Limits& limits = {}) const = 0;
    virtual std::string encode(const Value& value, const Limits& limits = {}) const = 0;
};
class JsonCodec final : public Codec {
public:
    Value decode(std::string_view bytes, const Limits& limits = {}) const override;
    std::string encode(const Value& value, const Limits& limits = {}) const override;
};
} // namespace equorus
