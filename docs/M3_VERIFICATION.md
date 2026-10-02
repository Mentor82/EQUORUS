# M3 verification — 2026-10-02

Implemented: [Go and Rust conformance for LiNeP-Ollama and L.I.A.R.A.-OS](../PLAN.md#m3--go-and-rust-conformance).
All four target languages (C++20, Python, Go, Rust) now independently implement the EQUORUS serialization contract, canonical byte profile (`equorus-value-v1`), detached SHA-256 integrity, and LiNeP v0.2 request projections.

## Executed environments

| Environment | Toolchain | Targets | Result |
| --- | --- | --- | --- |
| Windows x64 | Go 1.27.0 | `go/equorus`, `go/cmd/*` | All native & cross-language tests passed |
| Windows x64 | Rust 1.92.0 / Cargo 1.92.0 | `rust` (zero external dependencies) | All native & cross-language tests passed |
| Windows x64 | MSVC 19.51.36257.0, Python 3.12.7 | C++20 Core, Integrity, Codec | 5/5 CTest groups passed |

## Conformance matrix

| Test Suite / Capability | C++20 | Python (Ref) | Go (`LiNeP-Ollama`) | Rust (`L.I.A.R.A.-OS`) |
| --- | :---: | :---: | :---: | :---: |
| **Value Tree & Safe Integer ($\pm 2^{53}-1$)** | Pass | Pass | Pass | Pass |
| **Incremental Limit Enforcement** | Pass | Pass | Pass | Pass |
| **Duplicate Key & Lone Surrogate Rejection** | Pass | Pass | Pass | Pass |
| **`equorus-value-v1` Canonical Bytes** | Pass | Pass | Pass | Pass |
| **Detached SHA-256 Domain Integrity** | Pass | Pass | Pass | Pass |
| **Pilot Fixtures (VINOX, LIARA, LiNeP)** | Pass | Pass | Pass | Pass |
| **`test_conformance.py` (767 cases)** | Pass | Pass | Pass | Pass |
| **`test_integrity.py` (460 corruptions + 1000 binary64)** | Pass | Pass | Pass | Pass |
| **Native LiNeP v0.2 Request Adapter** | Pass | Pass | Pass | Pass |

## Implementation highlights

### Go (`go/`)
- Native Go module: `github.com/Mentor82/EQUORUS/go`
- Recursive-descent parser bounded by `Limits` before memory allocation.
- Exact IEEE-754 binary64 bit manipulation, subnormal support, and unsigned UTF-8 key sorting.
- 1:1 typed projection to `LiNeP-Ollama`'s `v02.RequestEnvelope` and `GenerationOptions`.
- CLI binaries: `equorus-roundtrip` and `equorus-integrity`.

### Rust (`rust/`)
- 100% Pure Rust Standard Library (`edition = "2021"`) with **Zero External Dependencies**, matching `L.I.A.R.A.-OS`'s `liara-noded` design constraints.
- Native pure Rust SHA-256 implementation with known-answer verification.
- Natural `BTreeMap` utilization for unsigned UTF-8 lexicographical key ordering.
- 1:1 typed projection to `L.I.A.R.A.-OS`'s `linep_v02::RequestEnvelope` and `Option<GenerationOptions>`.
- CLI binaries: `equorus_roundtrip` and `equorus_integrity`.

## Exit gate evaluation

- [x] Independent Go checks for LiNeP-Ollama implemented and passing.
- [x] Independent Rust checks for L.I.A.R.A.-OS implemented and passing.
- [x] Native implementations share the specification without linking C++ runtime.
- [x] uint64 IDs/seeds, float32 options, limits, and version/presence verified across all 4 runtimes.
- [x] C++, Python, Go, and Rust agree on supported values and canonical bytes.
- [x] Existing LiNeP v0.2 frames remain byte-compatible under adapter roundtrips.
