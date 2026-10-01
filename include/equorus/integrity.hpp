#pragma once
#include "equorus/envelope.hpp"

namespace equorus {
inline constexpr std::string_view canonical_profile = "equorus-value-v1";
inline constexpr std::string_view integrity_algorithm = "sha-256";

// Canonical hash representation, NOT a transport codec. Returns owned bytes.
std::string canonical_bytes(const Value&, std::string_view profile = canonical_profile,
                            const Limits& limits = {});

// Detached, public checksum metadata. No authentication or trust assertion.
struct IntegrityRecord {
    std::string profile;
    std::string algorithm;
    std::string digest; // exactly 64 lowercase hex characters
    bool operator==(const IntegrityRecord&) const = default;
};
IntegrityRecord compute_integrity(const Envelope&,
                                  std::string_view profile = canonical_profile,
                                  std::string_view algorithm = integrity_algorithm,
                                  const Limits& limits = {});
// False means a valid record with a mismatching digest. Invalid metadata throws.
bool verify_integrity(const Envelope&, const IntegrityRecord&, const Limits& limits = {});
std::string encode_integrity(const IntegrityRecord&, const Limits& limits = {});
IntegrityRecord decode_integrity(std::string_view json, const Limits& limits = {});
} // namespace equorus
