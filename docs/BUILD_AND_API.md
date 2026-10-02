# Build and API — v0.1 Release

Status: completed v0.1 release baseline. Implements owned core, production JSON codec, canonical bytes (`equorus-value-v1`), detached SHA-256 integrity, pure C ABI, consumer adapters (VINOX, LIARA, LiNeP v0.2), and 4-way cross-language conformance (C++20, Python, Go, Rust).

## Build and installation

### Prerequisites
- **C++ / C ABI**: CMake >= 3.20, C++20 compiler (MSVC 19.40+, GCC 13+, Clang 16+).
- **Python**: Python >= 3.10, jsonschema >= 4.0.
- **Go**: Go >= 1.22.
- **Rust**: Rust >= 1.80 (2021 edition), Cargo (zero external dependencies).

All C++ third-party headers (`nlohmann/json.hpp`, `picosha2/picosha2.h`) are vendored with verified SHA-256 hashes; builds require zero network access.

### C++ / C ABI CMake Build & Install

```bash
# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build targets and test executables
cmake --build build --config Release

# Run complete test suite (9 test groups)
ctest --test-dir build -C Release --output-on-failure

# Install libraries, headers, and CMake package config
cmake --install build --prefix /path/to/install
```

Installed artifacts:
- Headers: `<prefix>/include/equorus/` (`codec.hpp`, `envelope.hpp`, `equorus.hpp`, `integrity.hpp`, `pilot.hpp`, `value.hpp`, `equorus_c.h`, `adapters/linep.hpp`, `adapters/vinox.hpp`).
- Libraries: `<prefix>/lib/` (`libequorus.a`, `libequorus_json.a`, `libequorus_pilot.a`, `libequorus_integrity.a`, `libequorus_adapters.a`, `libequorus_c.a` or `.lib`).
- CMake config: `<prefix>/lib/cmake/equorus/` (`equorusConfig.cmake`, `equorusTargets.cmake`). Downstream CMake projects can use:
  ```cmake
  find_package(equorus REQUIRED)
  target_link_libraries(my_app PRIVATE equorus::core equorus::json equorus::integrity equorus::c)
  ```

### Go Package (`go/`)

```bash
cd go
go test -v ./...
go build ./cmd/equorus-roundtrip
go build ./cmd/equorus-integrity
```

### Rust Crate (`rust/`)

```bash
cd rust
cargo test --all-targets
cargo build --release
```
Maintains 100% Rust Standard Library with zero external dependencies, matching `liara-noded` conventions.

### Python Reference and Adapters (`python/`)

```bash
# Reference envelope validation and integrity
python tests/test_conformance.py <path-to-roundtrip-binary>
python tests/test_integrity.py <path-to-integrity-binary>

# LIARA Heartbeat opt-in adapter
python tests/test_liara_adapter.py
```

---

## C++ API Usage

```cpp
#include <equorus/pilot.hpp>
#include <equorus/integrity.hpp>
#include <equorus/adapters/vinox.hpp>
#include <equorus/adapters/linep.hpp>

// 1. Decode and encode with bounded limits
equorus::Limits limits{65536, 12, 2048, 8192};
auto env = equorus::pilot::decode(raw_json, "vinox.provenance.snapshot", limits);
std::string compact_json = env.encode(equorus::JsonCodec{}, limits);

// 2. Canonical byte serialization and detached integrity
std::string canon = equorus::canonical_bytes(env.value(), equorus::canonical_profile, limits);
auto integrity = equorus::compute_integrity(env, equorus::canonical_profile, equorus::integrity_algorithm, limits);
bool valid = equorus::verify_integrity(env, integrity, limits);

// 3. VINOX provenance adapter
vinox_provenance_meta meta{};
meta.struct_size = sizeof(meta);
meta.kind = VINOX_PROVENANCE_TOOL_EVIDENCE;
meta.source_id = "sensor:thermal";
meta.timestamp_ms = 1727800000000ULL;
auto vinox_env = equorus::adapters::vinox::to_envelope(meta, limits);

// 4. LiNeP v0.2 request adapter
equorus::adapters::linep::LinePRequestEnvelope req{};
req.stream = {1, 2, 0};
req.profile = equorus::adapters::linep::RuntimeProfile::chat;
req.model_id = "qwen2.5:7b";
req.payload = "Hello";
auto linep_env = equorus::adapters::linep::to_envelope(req, {{"kind", equorus::Value("SOURCE_LITERAL")}}, limits);
```

---

## C ABI Usage (`equorus/equorus_c.h`)

Opaque immutable handles, per-call status returns, and independent allocator boundaries:

```c
#include <equorus/equorus_c.h>

equorus_envelope* env = NULL;
equorus_status s = equorus_decode(
    bytes, len,
    "vinox.provenance.snapshot", 25,
    NULL, /* default limits */
    &env
);
if (s == EQUORUS_STATUS_OK) {
    equorus_buffer* buf = NULL;
    s = equorus_encode(env, EQUORUS_CODEC_CANONICAL_V1, NULL, &buf);
    if (s == EQUORUS_STATUS_OK) {
        const uint8_t* data = equorus_buffer_data(buf);
        size_t size = equorus_buffer_size(buf);
        /* use data */
        equorus_buffer_free(buf);
    }
    equorus_envelope_free(env);
}
```

---

## Compatibility Matrix (v0.1)

| Feature / Domain | Supported in v0.1 | Explicit Rejection / Failure Behavior | Deferred to Future Revisions |
| --- | --- | --- | --- |
| **Number representation** | Finite IEEE-754 binary64; integers in $[-2^{53}+1, 2^{53}-1]$ | `NaN`, `Infinity`, `-0.0`, float underflow, unsafe integers -> `NUMBER` error | Arbitrary precision decimals, BigInt beyond uint64 string |
| **uint64 domain fields** | Decimal strings without sign or leading zeros | Negative, float syntax, `> 2^64-1`, nonzero requirement for IDs -> `UINT64_RANGE` | Non-decimal string or binary uint64 |
| **float32 LiNeP options** | Exact binary32 widened to binary64 | Truncating or non-representable binary32 -> `FLOAT32` error | Lossy float narrowing |
| **String & Unicode** | UTF-8 scalar values, standard escapes | Lone surrogates, invalid UTF-8, BOM, unescaped controls (< 0x20) -> `UNICODE` / `MALFORMED` | Unicode normalization |
| **Resource limits** | Incremental bounds: bytes, depth, items, string lengths | Exceeding limit -> `LIMIT` error before allocation | Per-thread heap quota accounting |
| **LIARA attributes** | Strictly empty `{}` in pilot v0.1 | Non-empty attributes -> `SCHEMA` error (zero silent loss) | Typed heterogeneous attribute dictionary |
| **LiNeP options** | Explicit wire presence via `has_options`; canonical UTF-8 byte key ordering | Unsorted keys, duplicate keys -> `OPTION_KEYS` error | Arbitrary unsorted option maps |
| **Integrity metadata** | Detached `equorus-value-v1` + SHA-256 record | Corrupt digest or profile mismatch -> `verify_integrity` returns `false` | Inline signature envelopes, PKI certificates |
