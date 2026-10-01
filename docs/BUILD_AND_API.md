# Build and API (M1 + M2)

Status: implemented pilot core; public API is not frozen. JSON encoding is
ordinary encoding, not canonicalization or a LiNeP frame.

## Build and tests

Requirements: CMake >= 3.20, C++20 compiler, Python >= 3.10, jsonschema 4.x.
The C++ dependencies are vendored and the native build does not access the network.
Install Python development dependencies only if missing:

```text
python -m pip install -r tests/requirements.txt
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug --parallel 2
ctest --test-dir build -C Debug --output-on-failure
```

For MinGW/Linux Ninja, add `-G Ninja` in a new build directory.
Libraries without Python/tests: `-DBUILD_TESTING=OFF`.
Linux sanitizer build: `-DEQUORUS_SANITIZERS=ON`.
CI covers Windows/MSVC and Linux/GCC; a workflow file alone is not evidence of a
completed hosted run. Local/VM verification is recorded separately.

Targets: `equorus::core` (owned values and envelopes), `equorus::json` (JSON codec),
`equorus_pilot` (consumer-specific draft validators), `equorus::integrity`
(canonical bytes and detached SHA-256 records). The generic core does not
link to the JSON codec, load schemas or depend on a consumer repository.

## C++ API example

```cpp
#include <equorus/pilot.hpp>
equorus::Limits limits;
auto snapshot = equorus::pilot::decode(bytes, "vinox.provenance.snapshot", limits);
auto json = snapshot.encode(equorus::JsonCodec{}, limits);
```

`Envelope::decode` additionally accepts a codec, explicit supported schema versions
and a caller-supplied domain validator. Empty expected type or missing validator
is rejected. `Envelope::create` validates and copies a caller-built `Value` tree.
An envelope owns all strings/containers; its const view lasts until that envelope
is destroyed, moved from or assigned. Copying makes an independent snapshot.
The mutable builder may be edited without changing a created envelope.
Moved-from envelopes are only valid for destruction or reassignment.

The pilot number type is binary64; JSON integer/float spelling is not an invariant.
Bool is distinct from number; uint64 domain fields remain strings and
`parse_uint64` converts them exactly. Embedded NUL is a valid JSON string value.
No string view into input bytes is retained.

`Error` carries a category within this draft. See `ErrorCode` / `code_name`.
Syntax/budget checks precede envelope/consumer validation. For inputs violating
multiple rules, the first detected error can differ between implementations;
single-fault fixtures specify exact categories. Allocation failure propagates as
`std::bad_alloc`; it is not disguised as a contract error.

`JsonCodec` uses an allocation-free grammar/budget preflight, then nlohmann/json
SAX to build owned values and reject duplicate keys. This bounds depth, total
value count and decoded string byte length before SAX token/DOM allocations.
Input is already buffered; caller-side stream/framing limits still apply.
All input bytes, including trailing whitespace, count toward max_bytes.
The encoder validates the tree before writing and bounds output bytes
incrementally. Native create has no encoded-byte size until a codec is selected.

The hard depth ceiling is 128 (requesting a larger limit fails). NaN, infinities,
negative zero, unsafe integers and nonzero numeric tokens that underflow binary64
are rejected. Resource budgets do not promise an exact heap byte quota.
Allocator accounting, fuzz campaigns and deployment review remain M5.

## Python reference

Add `python/` to the import path and use `equorus_reference.Envelope`:

```python
snapshot = Envelope.decode(raw_bytes, "vinox.provenance.snapshot")
encoded = snapshot.encode()
```

Constructing `Envelope(value, expected_type)` freezes an independent native tree.
The `value` property returns a detached copy. The reference uses its own bounded
parser, the standard JSON string decoder, local schemas and independent semantic
checks. It does not import the fixture checker or bind to C++.
This is a repository development module, not a published Python package.

## Test boundaries

- Core: C++ snapshots/ownership, explicit versions/types, codec roundtrip,
  integer extremes, invalid export values and early resource abort.
- Cross-language: shared cases, bidirectional C++/Python roundtrips, native
  Python snapshots and adversarial bytes.
- Fixture contracts: original JSON Schema/semantic specification checks.
- Integrity native: SHA-256 known answers, canonical values, metadata and budgets.
- Integrity cross-language: published bytes/digests, numeric patterns and corruption.

Native means EQUORUS value/envelope objects. No VINOX, LIARA or LiNeP runtime is
loaded. The [M2 contract](CANONICAL_INTEGRITY_V1.md) documents canonical bytes and
integrity APIs. Consumer adapters, wire golden frames and Go/Rust remain later
milestones. The C ABI is a
[design](C_ABI_DESIGN.md), not an implemented exported interface.
