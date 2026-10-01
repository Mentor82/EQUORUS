#include "equorus/integrity.hpp"
#include "equorus/pilot.hpp"
#include <picosha2/picosha2.h>
#include <functional>
#include <iostream>
#include <limits>
using namespace equorus;
void require(bool ok) { if (!ok) throw std::runtime_error("integrity assertion failed"); }
void rejects(ErrorCode code, const std::function<void()>& f) {
    try { f(); } catch (const Error& e) { require(e.code() == code); return; }
    throw std::runtime_error("expected rejection");
}
int main() {
    try {
        // SHA-256 known answers independent of EQUORUS and Python.
        require(picosha2::hash256_hex_string(std::string()) ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        require(picosha2::hash256_hex_string(std::string("abc")) ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        require(picosha2::hash256_hex_string(std::string(1000000, 'a')) ==
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
        auto env = pilot::decode(R"({"type_id":"vinox.provenance.snapshot","schema_version":"0.1","provenance":{"kind":"SOURCE_LITERAL"},"payload":{}})",
                                 "vinox.provenance.snapshot");
        const auto record = compute_integrity(env);
        require(verify_integrity(env, record));
        require(decode_integrity(encode_integrity(record)) == record);
        auto wrong = record;
        wrong.digest[0] = wrong.digest[0] == '0' ? '1' : '0';
        require(!verify_integrity(env, wrong));
        wrong.profile = "json-jcs-v1";
        rejects(ErrorCode::profile, [&] { verify_integrity(env, wrong); });
        rejects(ErrorCode::profile, [&] { canonical_bytes(env.value(), ""); });
        rejects(ErrorCode::algorithm, [&] { compute_integrity(env, canonical_profile, "sha-1"); });
        wrong = record; wrong.digest = "ABC";
        rejects(ErrorCode::integrity, [&] { verify_integrity(env, wrong); });
        const auto bytes = canonical_bytes(env.value());
        require(canonical_bytes(env.value(), canonical_profile, Limits{bytes.size(),12,2048,8192}) == bytes);
        rejects(ErrorCode::limit, [&] {
            compute_integrity(env, canonical_profile, integrity_algorithm, Limits{bytes.size()-1,12,2048,8192});
        });
        require(canonical_bytes(Value(0.0)) == std::string("d\0\0\0\0\0\0\0\0", 9));
        require(canonical_bytes(Value(std::numeric_limits<double>::denorm_min())) ==
                std::string("d\0\0\0\0\0\0\0\1", 9));
        rejects(ErrorCode::number, [] { canonical_bytes(Value(-0.0)); });
        rejects(ErrorCode::number, [] { canonical_bytes(Value(std::numeric_limits<double>::quiet_NaN())); });
        rejects(ErrorCode::unicode, [] { canonical_bytes(Value(std::string("\xff"))); });
        rejects(ErrorCode::limit, [] { canonical_bytes(Value(nullptr), canonical_profile, Limits{0,12,2048,8192}); });
        rejects(ErrorCode::limit, [] { canonical_bytes(Value(Value::Array{Value(nullptr)}), canonical_profile, Limits{65536,1,2048,8192}); });
        rejects(ErrorCode::limit, [] { canonical_bytes(Value("abc"), canonical_profile, Limits{65536,12,2048,2}); });
        rejects(ErrorCode::limit, [] { canonical_bytes(Value(Value::Array{Value(nullptr)}), canonical_profile, Limits{65536,12,1,8192}); });
        std::cout << "integrity native tests and SHA-256 known answers passed\n";
    } catch (const std::exception& e) { std::cerr << e.what(); return 1; }
}
