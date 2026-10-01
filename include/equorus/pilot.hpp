#pragma once
#include "equorus/envelope.hpp"
namespace equorus::pilot {
// Draft consumer validation is separate from the generic core and codec.
void validate(const Value&);
Envelope decode(std::string_view bytes, std::string_view expected_type, const Limits& limits = {});
Envelope create(const Value&, std::string_view expected_type, const Limits& limits = {});
}
