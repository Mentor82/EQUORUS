# EQUORUS v0.1 Release Gate Report

Date: 2026-10-02  
Status: **PASSED / READY FOR TAGGING**  
Version: `0.1.0`  
Tagline: *Different representations. Equivalent information.*

---

## 1. Executive Summary

Milestone **M5 (v0.1 release gate)** is complete. All 6 planned milestones (M0 through M5) have been designed, implemented, cross-language verified, and documented according to the architecture principles in `PLAN.md`.

EQUORUS provides an independent, allocation-bounded envelope and canonical integrity layer across heterogeneous runtimes without forcing monolithic dependencies, without mutating wire formats, and without breaking existing transport protocols or OS signature preimages.

---

## 2. Milestone Roadmap Completion

| Milestone | Title | Status | Artifacts & Evidence |
| --- | --- | :---: | --- |
| **M0** | Planning and contracts | Complete | `PLAN.md`, `schemas/pilot-v0.1/`, `tests/fixtures/pilot-v0.1/` |
| **M1** | Minimal reference core | Complete | `docs/M1_VERIFICATION.md`, C++20 Core, Python Reference |
| **M2** | Canonical profile and detached integrity | Complete | `docs/M2_VERIFICATION.md`, `CANONICAL_INTEGRITY_V1.md`, `equorus-value-v1` |
| **M3** | Go and Rust conformance | Complete | `docs/M3_VERIFICATION.md`, `go/equorus`, `rust` (0 external deps) |
| **M4** | Opt-in consumer integration | Complete | `docs/M4_VERIFICATION.md`, C ABI, VINOX, LIARA, LiNeP v0.2 adapters |
| **M5** | v0.1 release gate | Complete | `docs/RELEASE_V0_1.md`, `docs/BUILD_AND_API.md`, CMake packaging & install |

---

## 3. Release Gate Evaluation Criteria

### Criterion 1: Reviewed envelope/schema contracts and compatibility matrix
- **Envelopes**: Every envelope strictly enforces `type_id`, `schema_version = "0.1"`, `provenance`, and `payload`.
- **Pilot Schemas**: `vinox.provenance.snapshot`, `liara.heartbeat.snapshot`, `linep.v02.request`.
- **Compatibility Matrix**: Explicitly defined in [`docs/BUILD_AND_API.md`](BUILD_AND_API.md). Numbers are finite IEEE-754 binary64; integers restricted to $[-2^{53}+1, 2^{53}-1]$; negative zero, NaN, infinities, and underflowing floats are rejected. uint64 fields use decimal strings without leading zeros. LiNeP float32 fields preserve exact binary32 widened to binary64. Attributes in LIARA heartbeat observations must be `{}` (rejecting non-empty attributes to prevent silent data loss).

### Criterion 2: Tested C++ API and C ABI lifetimes, allocation and error handling
- **C ABI**: Declared in [`include/equorus/equorus_c.h`](../include/equorus/equorus_c.h) and implemented in [`src/c_api.cpp`](../src/c_api.cpp).
- **Ownership & Allocators**: Opaque handles (`equorus_envelope`, `equorus_buffer`) guarantee independent allocator boundaries. Callers never free internal memory directly. Buffers outlive the envelopes that produced them.
- **Concurrent Reads**: Stress-tested with multi-threaded concurrent readers from shared immutable envelope handles in strict ISO-C (`tests/c_abi_tests.c`).
- **Error Codes**: Frozen, discrete status codes (`EQUORUS_STATUS_OK` through `EQUORUS_STATUS_INTERNAL_ERROR`). Output pointers are guaranteed to remain null on error.

### Criterion 3: Production JSON codec with incremental resource limits and fuzz coverage
- **Production Codec**: [`codecs/json.cpp`](../codecs/json.cpp) utilizes an allocation-free grammar and budget preflight (`Guard`) before SAX token or DOM allocation occurs.
- **Resource Limits**: Four explicit bounds enforced at each parsing step: `max_bytes`, `max_depth`, `max_items`, `max_string_length`.
- **Fuzzing & Stress Testing**: [`tests/fuzz_limits_tests.cpp`](../tests/fuzz_limits_tests.cpp) executes 1,000 randomized mutations (bit flips, deletions, insertions, truncations) and extreme structural stress cases (nesting depths up to 2,000, 100,000 item arrays, oversized strings). All inputs either decode cleanly or fail with expected error codes; zero crashes or memory corruption observed.
- **Experimental Parser**: `experimental/json_parser_seed/` remains isolated for research and is not linked into the production distribution.

### Criterion 4: Canonical byte and integrity fixtures, including corruption cases
- **Profile**: `equorus-value-v1` defines deterministic, unique serialization with unsigned UTF-8 byte key ordering, exponential notation for non-integer numbers, and no whitespace.
- **Detached SHA-256 Record**: Independent digest metadata (`compute_integrity`, `verify_integrity`).
- **Test Corpus**: 17 canonical vectors in `tests/fixtures/canonical-v1/`, 6 envelope goldens, 460 corruption mutations, and 1,000 binary64 numbers verified identically across C++, Python, Go, and Rust.

### Criterion 5: C++/Python/Go/Rust conformance for advertised subset
All four implementations pass 100% of their test suites:
- **C++20**: MSVC and GCC/Clang via CMake.
- **Python**: Reference decoder/encoder and LIARA adapter.
- **Go**: `go/equorus` native parser, writer, canonical integrity, and LiNeP-Ollama adapter.
- **Rust**: `rust` crate with 100% standard library (zero external dependencies), matching `L.I.A.R.A.-OS`'s `liara-noded` conventions.

### Criterion 6: Consumer integration evidence, reproducible builds, and installation docs
- **VINOX Provenance**: C++ adapter (`adapters/vinox.hpp`) and C ABI functions (`equorus_vinox_provenance_*`) pass minimal, full, and null metadata roundtrips without synthesizing timestamps.
- **LIARA Heartbeat**: Python adapter (`python/equorus_liara.py`) passes microsecond and timezone preservation against `tests/fixtures/pilot-v0.1/heartbeat.json`.
- **LiNeP v0.2 Request**: C++, Go, and Rust adapters pass roundtrips against `linep-options.json` and `linep-no-options.json` with canonical extra options sorting.
- **Installation Support**: CMake `install(TARGETS ... EXPORT equorusTargets)` generates headers, static libraries, and package config (`equorusConfig.cmake`, `equorusTargets.cmake`) allowing downstream projects to link via `find_package(equorus)`.

---

## 4. Test Verification Summary

| Test Group | Runner | Test Cases / Checks | Result |
| --- | --- | --- | :---: |
| **`core`** | C++ CTest | Snapshot ownership, copy semantics, number bounds, limits | **PASS** (0.01s) |
| **`cross_language`** | CTest / Python | 767 cases (shared suite, bidirectional, snapshots, adversarial) | **PASS** (8.45s) |
| **`fixture_contracts`** | CTest / Python | Schema and semantic pilot fixtures validation | **PASS** (0.26s) |
| **`integrity_native`** | C++ CTest | SHA-256 known answers, canonical vectors, limit rejections | **PASS** (0.31s) |
| **`integrity_cross_language`** | CTest / Python | 17 canonical vectors, 6 goldens, 460 corruptions, 1000 numbers | **PASS** (6.92s) |
| **`adapters_native`** | C++ CTest | VINOX and LiNeP v0.2 C++ adapter roundtrips & rejections | **PASS** (0.02s) |
| **`c_abi_smoke`** | C CTest | Pure C ABI, limits, memory lifecycles, multithreaded concurrent reads | **PASS** (0.09s) |
| **`liara_heartbeat_adapter`** | CTest / Python | LIARA HeartbeatSnapshot microsecond roundtrips & invariants | **PASS** (0.62s) |
| **`fuzz_limits`** | C++ CTest | Depth/item/string limits stress & 1000 randomized fuzz mutations | **PASS** (0.06s) |
| **`go test ./...`** | Go Test | 86 pilot cases, canonical vectors, LiNeP roundtrip, extra options | **PASS** (0.53s) |
| **`cargo test`** | Cargo | Known SHA-256, canonical vectors, LiNeP roundtrip, extra options | **PASS** (0.01s) |

**Total CTest Execution Time:** 16.76s  
**Test Success Rate:** 100% (0 failures across all 11 test groups)

---

## 5. Sign-Off & Recommendation

All exit conditions specified in `PLAN.md` for **M5 — v0.1 release gate** are satisfied. The codebase is ready for v0.1.0 tagging and consumer integration deployment.
