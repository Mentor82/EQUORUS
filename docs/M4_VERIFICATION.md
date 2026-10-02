# M4 verification — 2026-10-02

Implemented: [Opt-in consumer integration](../PLAN.md#m4--opt-in-consumer-integration).
Completed opt-in consumer adapters and the pure C ABI surface across VINOX provenance, LIARA runtime heartbeat, and LiNeP v0.2 request projections. Existing consumer wire formats, signature preimages, and transport framing remain unmodified and fully rollback-safe.

## Executed environments

| Environment | Toolchain | Targets | Result |
| --- | --- | --- | --- |
| Windows x64 | MSVC 19.51.36257.0 | `equorus_adapters`, `equorus_c`, `equorus_adapter_tests`, `equorus_c_abi_tests` | 8/8 CTest groups passed (100%) |
| Windows x64 | Python 3.12.7 | `python/equorus_liara.py`, `tests/test_liara_adapter.py` | All adapter tests passed |
| Windows x64 | Go 1.27.0 | `go/equorus/linep.go`, `go/equorus/equorus_test.go` | All native & roundtrip tests passed |
| Windows x64 | Rust 1.92.0 / Cargo 1.92.0 | `rust/src/linep.rs`, `rust/tests/conformance.rs` (zero external dependencies) | 5/5 native tests passed |

## Consumer adapter matrix

| Consumer | Native Contract | EQUORUS Envelope | Language / Surface | Verified Invariants |
| --- | --- | --- | --- | --- |
| **VINOX** | `vinox_provenance_meta` (`vinox.h`) | `vinox.provenance.snapshot` | C++20 (`adapters/vinox.hpp`) & C ABI (`equorus_c.h`) | ABI field availability (`struct_size >= 8`), `kind` 0..3 mapping, null vs string `source_id`, uint64 decimal `timestamp_ms`, no timestamp synthesized |
| **LIARA** | `HeartbeatSnapshot` (`heartbeat.py`) | `liara.heartbeat.snapshot` | Python (`python/equorus_liara.py`) | Microsecond timestamp fidelity, uint64 `sequence` range `[0, 2^64-1]`, strict empty `attributes` invariant, metric-unit validation |
| **LiNeP v0.2** | `request_envelope` (`envelopes.hpp`) | `linep.v02.request` | C++20 (`adapters/linep.hpp`) | Exact float32 widening (`0.9f` -> `0.8999999761581421`), canonical UTF-8 byte extra option sorting, duplicate key rejection, `has_options` flag integrity |
| **LiNeP-Ollama** | `RequestEnvelope` (`types.go`) | `linep.v02.request` | Go (`go/equorus/linep.go`) | Stream identity nonzero validation, canonical extra options sorting, duplicate key rejection, roundtrip equivalence |
| **L.I.A.R.A.-OS** | `linep_v02::RequestEnvelope` | `linep.v02.request` | Rust (`rust/src/linep.rs`) | Pure Rust std library (0 external crates), canonical extra option sorting, duplicate key rejection, roundtrip equivalence |

## Implementation details

### 1. VINOX C++ Adapter & C ABI Surface (`include/equorus/adapters/vinox.hpp`, `include/equorus/equorus_c.h`, `src/c_api.cpp`)
- Defined C ABI header `equorus_c.h` with independent opaque handles (`equorus_envelope`, `equorus_buffer`), numeric error codes, `struct_size` and `abi_version` checks, and detached SHA-256 integrity computation.
- Pure C smoke test (`tests/c_abi_tests.c`) compiled in strict C mode (`/TC`) confirms:
  - Null pointer and zero-length distinctions.
  - Rejection of short options structures (`struct_size < MIN_SIZE`) and incompatible ABI versions.
  - Safe memory boundaries: callers never allocate or free library storage directly; buffer outlives envelope destruction.
  - VINOX metadata mapping handles minimal structs (`struct_size = 8`), null `source_id`, and full structs with uint64 timestamps.

### 2. LIARA Heartbeat Python Adapter (`python/equorus_liara.py`, `tests/test_liara_adapter.py`)
- Direct bidirectional projection between Pydantic `HeartbeatSnapshot` and `liara.heartbeat.snapshot`.
- Exact numeric preservation: Python arbitrary-precision integer `sequence` maps to uint64 decimal string without floating-point conversion.
- Rejection of non-empty `attributes`: ensures no silent data loss during pilot exchange.
- Microsecond and timezone preservation: retains explicit ISO 8601 formatting.

### 3. LiNeP v0.2 Adapters in C++, Go, and Rust
- C++ adapter (`include/equorus/adapters/linep.hpp`, `src/adapters.cpp`) verified against golden fixtures `linep-options.json` and `linep-no-options.json`.
- Lexicographical UTF-8 byte ordering for `extra_options` enforced uniformly across C++, Go, and Rust.
- Duplicate key rejection validated across all three implementations.

## Test summary

```text
CTest in build-msvc:
  1/8 Test #1: core .............................   Passed    0.02 sec
  2/8 Test #2: cross_language ...................   Passed   22.40 sec
  3/8 Test #3: fixture_contracts ................   Passed    0.43 sec
  4/8 Test #4: integrity_native .................   Passed    0.43 sec
  5/8 Test #5: integrity_cross_language .........   Passed   12.46 sec
  6/8 Test #6: adapters_native ..................   Passed    0.03 sec
  7/8 Test #7: c_abi_smoke ......................   Passed    0.03 sec
  8/8 Test #8: liara_heartbeat_adapter ..........   Passed    0.87 sec
100% tests passed, 0 tests failed out of 8 (Total Test time: 36.71 sec)

Go tests:
  go test -v ./... -> PASS (TestPilotCases, TestLinePAdapterRoundtrip, TestLinePAdapterExtraOptions)

Rust tests:
  cargo test -> PASS (5/5 tests: known SHA-256, error rejections, extra_options sorting/duplicates, linep roundtrip, canonical vectors)

Cross-Language Conformance & Integrity:
  767 test_conformance.py cases: PASS on C++, Go, and Rust
  460 corruption checks + 1000 binary64 values in test_integrity.py: PASS on C++, Go, and Rust
```

## Exit gate evaluation

- [x] VINOX provenance export/import pilot implemented and verified in C++ and C ABI.
- [x] LIARA heartbeat export/import pilot implemented and verified with microsecond fidelity.
- [x] LiNeP v0.2 runtime adapters verified across C++, Go, and Rust with canonical options sorting and golden frame equivalence.
- [x] Existing consumer wire formats and OS signature preimages left untouched (purely opt-in projection).
- [x] Real adapter roundtrips pass 100%; unsupported features fail clearly.
