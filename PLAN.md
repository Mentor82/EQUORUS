# EQUORUS — Implementation Plan

Status: pre-v0.1; M1/M2 implemented and verified on Windows and VM108. Hosted CI pending. Updated 2026-10-01.

Different representations may change; contract-equivalent information must not.

## Scope and ownership

EQUORUS owns the object envelope, lossless value representation, explicit schema
compatibility, codec boundaries, resource limits and conformance fixtures.
Consumers own domain validation, authorization, evidence assessment and migrations.

All LiNeP integration targets **LiNeP v0.2**. LiNeP owns headers, framing, stream
identities, lifecycle, leases and transport security. LiNeP-SL is a separate
security layer with its own version/profile. An EQUORUS schema version must
never imply a LiNeP or LiNeP-SL version.

**L.I.A.R.A.-Cluster and its Cluster OS/Buildroot line are excluded.**
L.I.A.R.A.-OS is the infrastructure/OS consumer. Go remains relevant through
LiNeP-Ollama; Rust through L.I.A.R.A.-OS. Cluster audit and package schemas are
not requirements for EQUORUS.

| Consumer | Initial boundary | Ownership retained by consumer |
| --- | --- | --- |
| VINOX | Provenance metadata; manifests later | C ABI, inference and model lifecycle |
| L.I.A.R.A. | Heartbeat snapshot; evidence/memory later | Units, evidence states, confidence, governance |
| LiNeP v0.2 / LiNeP-Ollama | Request projection and conformance | Wire codec, options semantics, execution and security |
| LiNeP-llamacpp / LiNeP-llama.cpp | Later C++ adapter | Runtime integration; select/pin intended repository first |
| L.I.A.R.A.-OS | Rust conformance, later package/runtime integration | Node authority, existing signatures and OS lifecycle |
| Personal / www.mw-dresden.de | Later explicit adapters | User boundaries, memory meaning, HTTP/OpenAPI contracts |
| AI-Training / Industrial_KI / MW-PyWebServer | Only for a concrete later exchange | Dataset, industrial protocol and event semantics |
| LiNeP-TensorRT / LiNeP-vllm / Spielzimmer / Codex | No initial implementation requirement | Outside the first milestones |

## M0 — Reviewable consumer contracts (accepted baseline)

- [x] Record scope and pinned source references.
- [x] Specify three independent exchange types: VINOX provenance, LIARA heartbeat,
  LiNeP v0.2 request.
- [x] Add draft JSON Schemas, synthetic examples and executable rejection cases.
- [x] Define pilot number, presence, version, unknown-field and limit rules.
- [x] User accepted these mappings as the implementation baseline; public API
  freeze remains the v0.1 release gate.

Deliverables: [consumer contracts](docs/CONSUMER_CONTRACTS_V0_1.md),
`schemas/pilot-v0.1/`, `tests/fixtures/pilot-v0.1/` and
`tests/validate_contract_fixtures.py`.

Exit: the fixture checker passes; each field has an explicit mapping and each
unsupported input fails without lossy coercion. This proves fixture consistency,
not a production decoder, native roundtrip or wire compatibility.

## M1 — C++ core and independent Python reference

Implementation: [build/API guide](docs/BUILD_AND_API.md),
[C ABI design](docs/C_ABI_DESIGN.md), C++ core/codec/pilot libraries and independent
Python reference. Windows/Linux CI workflow is included; hosted CI has not run
as part of this local task. See [verification record](docs/M1_VERIFICATION.md).

Build C++20 library/test targets with CMake and Windows/Linux CI. Implement owned,
snapshot-friendly values, envelope validation and a replaceable ordinary JSON
codec. Design C ABI ownership/error/buffer rules before consumer linking.

Implement an independent Python reference against the same fixtures. Test
native object -> JSON -> native object, including null/absence, uint64 boundaries,
finite floating point, Unicode, duplicate keys and schema mismatches. Production
decoders enforce limits during parsing, before excessive allocation.

Exit: both implementations preserve declared values and reject the same invalid
inputs. No consumer migration is required. The experimental parser stays isolated.

## M2 — Canonical bytes and integrity

Implemented profile: `equorus-value-v1`, with exact binary64 bits and unsigned
UTF-8 key ordering. Detached `sha-256` records cover the whole envelope and bind
the profile/algorithm through a domain-separated preimage. See the normative
[M2 contract](docs/CANONICAL_INTEGRITY_V1.md). JCS/RFC 8785 remains a candidate,
not an implemented promise. No struct layout is hashed.
Executed checks: [M2 verification](docs/M2_VERIFICATION.md).

Exit: C++/Python produce identical published byte fixtures; changes to covered
data fail verification. Unsupported profiles fail explicitly. Existing LiNeP-SL
MACs and OS package signatures remain owned by their protocols.

## M3 — Go and Rust conformance

Add independent Go checks for LiNeP-Ollama and Rust checks for L.I.A.R.A.-OS.
Native implementations may share a specification without linking a C++ runtime.
Exercise uint64 IDs/seeds, float32 options, limits and version/presence behavior.
Reuse pinned LiNeP v0.2 wire golden frames in LiNeP adapter tests, separately
from EQUORUS object fixtures.

Exit: C++, Python, Go and Rust agree on supported values and canonical bytes.
Existing LiNeP v0.2 frames remain byte-compatible under adapter tests.

## M4 — Opt-in consumer integration

1. VINOX provenance and LIARA heartbeat export/import pilots.
2. LiNeP v0.2 runtime adapters and L.I.A.R.A.-OS after M3.
3. Personal and website only through explicit domain/API mappings.
4. Other projects only when a real exchange case justifies them.

Keep existing paths available for comparison and rollback. Do not inject an
EQUORUS envelope into a LiNeP payload without an explicitly supported application
mapping. A request projection is not a new wire format. Do not convert Personal
confidence to Core confidence without an agreed domain rule. Do not replace
existing OS package signature preimages.

Exit: real adapter roundtrips and existing consumer tests pass; unsupported
features fail clearly; opt-in can be disabled without migrating stored data.

## M5 — v0.1 release gate

- Reviewed envelope/schema contracts and compatibility matrix.
- Tested C++ API and C ABI lifetimes, allocation and error handling.
- Production JSON codec with incremental resource limits and fuzz coverage.
- Canonical byte/integrity fixtures, including corruption cases.
- C++/Python/Go/Rust conformance for the advertised subset.
- Consumer integration evidence, reproducible builds and installation docs.

No binary codec, compression framework, registry service, reflection system,
PKI policy or wholesale consumer migration is required for v0.1.

## Experimental parser

`experimental/json_parser_seed/` remains research-only. Promotion requires
conformance, Unicode/number/duplicate-key review, bounded allocation, fuzzing,
benchmarks and an explicit architecture decision. It is not on the critical path.
