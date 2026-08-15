# EQUORUS — Implementation Plan

> **Project premise:** Different representations may change; contract-equivalent information must not.

## 0. Scope

EQUORUS is a serialization and canonical representation layer. It is **not** a transport protocol, message broker, governance engine, database, or model runtime.

### In scope

- canonical object envelope
- type identity
- schema versioning/evolution
- provenance preservation
- codec abstraction
- deterministic/canonical encoding profiles
- integrity metadata over encoded representations
- decode resource limits
- C++20 core with stable C ABI planned
- conformance and cross-language fixtures

### Explicit non-goals for v0.1

- replacing LiNeP framing or transport security
- making LIARA governance decisions
- replacing VINOX inference/model lifecycle
- schema registry service
- Protobuf/Avro/MessagePack support before a real consumer requires it
- compression framework
- signing/PKI framework
- arbitrary object reflection

## 1. Phase 0 — Architecture seed

**Goal:** Freeze terminology before implementation spreads.

Deliverables:

- `docs/SERIALIZATION_CONTRACT_V0_1.md`
- object-envelope terminology
- provenance rules
- schema-version compatibility rules
- canonicalization profile concept
- explicit boundaries to LIARA, LiNeP, and VINOX
- preserve the VINOX JSON parser spike under `experimental/`

Exit criteria:

- one unambiguous definition of serialization vs codec vs transport
- no dependency on a particular JSON implementation in the core contract

## 2. Phase 1 — Core object model

Introduce a small C++20 model for:

```text
ObjectEnvelope
  type_id
  schema_version
  provenance
  payload
  canonical_profile (optional)
  integrity (optional)
```

Requirements:

- immutable/snapshot-friendly value semantics
- explicit byte/string ownership
- bounded lengths
- no transport-specific fields
- deterministic validation errors

A stable C ABI must be designed before consumers are asked to link against the library.

## 3. Phase 2 — Codec interface

Define codec operations conceptually as:

```text
encode(object, profile, limits) -> bytes
decode(bytes, expected_type, supported_versions, limits) -> object
```

Initial production codec candidate: JSON.

The codec implementation must remain replaceable. Codec choice must never define object identity or provenance semantics.

## 4. Phase 3 — Versioning and evolution

Define compatibility behavior for:

- same major / additive minor evolution
- unknown optional fields
- unknown required fields
- incompatible major versions
- downgrade/upgrade policy

Default safety rule: incompatible or ambiguous decoding fails closed.

## 5. Phase 4 — Canonicalization

Add explicit canonical profiles only where deterministic bytes are required.

Candidate first profile:

- canonical JSON profile compatible with a documented standard such as JCS/RFC 8785, subject to implementation review.

Canonicalization must be opt-in and named. Ordinary encoding and canonical encoding are different operations.

## 6. Phase 5 — Provenance and integrity

Preserve provenance classes compatible with the requirement first identified in VINOX:

- source literal
- tool evidence
- model generated
- derived context

Rules:

- serialization does not silently reclassify provenance
- transformation creates a new derived object instead of mutating source provenance
- integrity metadata is computed over defined encoded bytes
- hash/MAC/signature algorithms remain policy choices outside the base object semantics

## 7. Phase 6 — Consumer adapters

Adapters and conformance fixtures for real consumers:

### LIARA

- versioned Pydantic/service-contract fixture
- memory/audit object fixture
- provenance-preserving round trip

### LiNeP

- semantic payload -> encoded bytes -> LiNeP payload boundary
- retain LiNeP header, sequence, CRC/MAC and fragmentation ownership in LiNeP

### VINOX

- C/C++ provenance envelope fixture
- model-manifest/structured request fixture
- future MCP/HTTP payload compatibility evaluation

## 8. Phase 7 — Cross-language conformance

Golden fixtures must prove equivalent information across at least:

- C++
- Python

Future bindings may be added only against the same object and canonicalization contracts.

## 9. Experimental parser research

`experimental/json_parser_seed/` is intentionally separate from the production codec path.

Research topics:

- RFC 8259 conformance
- UTF-16 surrogate pairs / UTF-8 correctness
- duplicate-key policy
- maximum nesting depth
- allocation strategy / arena allocation
- DOM vs SAX/streaming modes
- deterministic memory ceilings
- fuzzing
- benchmark comparison against established libraries
- SIMD opportunities

Promotion into production requires benchmarks, conformance evidence, fuzzing/security review, and an explicit architecture decision.

## 10. Definition of done for v0.1

EQUORUS v0.1 is ready when:

- object envelope and terminology are frozen
- public C++ API and C ABI are documented and tested
- at least one production codec exists
- schema/version compatibility tests exist
- provenance round-trip tests exist
- canonical byte fixtures exist
- decode resource-limit tests exist
- LIARA, LiNeP and VINOX integration fixtures demonstrate the boundary without coupling EQUORUS to those projects

---

**Guiding invariant:** Representation may change. Contract-equivalent information must survive.

— Nephy 🔎
