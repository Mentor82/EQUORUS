# EQUORUS

**Different representations. Equivalent information.**

EQUORUS is a transport-independent, version-aware serialization and canonical representation platform for preserving **identity, structure, provenance, and integrity** across representation boundaries.

The project originated from a shared requirement observed across LIARA, LiNeP, and VINOX: objects may move between languages, processes, storage layers, codecs, and wire formats while the contract-relevant information must remain equivalent.

## Core idea

```text
Domain object
    |
    v
Canonical object contract
    |-- type identity
    |-- schema version
    |-- provenance
    |-- payload
    `-- optional integrity metadata
    |
    v
Codec
(JSON / binary / future codecs)
    |
    v
Encoded bytes (canonical only with an explicit profile)
    |
    +--> persistence
    +--> hashing/signing (requires a defined byte profile)
    `--> transport (e.g. LiNeP)
```

EQUORUS deliberately separates **object semantics** from **encoding format**. JSON is a codec, not the serialization architecture itself.

## v0.1 principles

1. Every serializable object has an explicit `type_id` and `schema_version`.
2. Decoding is type-aware and version-aware.
3. Provenance is preserved across serialization boundaries and is never silently rewritten.
4. Canonicalization is explicit and profile-driven where byte-for-byte reproducibility is required.
5. Integrity is calculated over defined encoded bytes, not incidental in-memory layouts.
6. Resource limits are part of the decode contract.
7. Transport framing, governance decisions, and domain semantics remain outside EQUORUS.

See [`docs/SERIALIZATION_CONTRACT_V0_1.md`](docs/SERIALIZATION_CONTRACT_V0_1.md) for the first common contract and [`PLAN.md`](PLAN.md) for the staged implementation plan.

## Intended consumers

- **LIARA** — versioned service contracts, memory/audit objects, orchestrator state and provenance-sensitive data.
- **LiNeP v0.2** — stable mapping between semantic payload objects and existing transport/wire representations; Go conformance through LiNeP-Ollama.
- **VINOX** — native C/C++ structured data, manifests, provenance envelopes, and future API/MCP payloads.

EQUORUS is independent of its consumers. Rust conformance and OS integration target
**L.I.A.R.A.-OS**; **L.I.A.R.A.-Cluster / Cluster OS is excluded** from this plan.

The [draft consumer contracts](docs/CONSUMER_CONTRACTS_V0_1.md) include pinned
source references, JSON Schemas and synthetic fixtures for VINOX provenance,
LIARA heartbeat and LiNeP v0.2 requests. See [fixture validation](tests/README.md).

## Experimental seed

The repository contains an experimental JSON parser seed under `experimental/json_parser_seed/`. It originated as a VINOX Phase 4 implementation spike and is preserved here as research material. It is **not** the production JSON foundation of EQUORUS and is not approved for untrusted network input.

## Status

**Pre-v0.1 / M2 implementation.** Owned C++20 values, immutable envelope snapshots,
a bounded JSON codec and an independent Python reference are available against
the pilot fixtures. See [build and API](docs/BUILD_AND_API.md) and
[M1 verification](docs/M1_VERIFICATION.md). M2 adds the explicit
[`equorus-value-v1` byte profile and detached SHA-256 checks](docs/CANONICAL_INTEGRITY_V1.md),
with independent C++/Python implementations and published golden vectors.
Consumer runtime adapters, exported C ABI and Go/Rust implementations are later work.
Public API and object contracts are not frozen.

## License

Apache License 2.0. See [`LICENSE`](LICENSE).

---

Architecture seed and review lineage: **Mirko & Nephy 🔎**
