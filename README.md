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
Canonical bytes
    |
    +--> persistence
    +--> hashing/signing
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
- **LiNeP** — stable mapping between semantic payload objects and transport/wire representations.
- **VINOX** — native C/C++ structured data, manifests, provenance envelopes, and future API/MCP payloads.

EQUORUS is independent of all three projects. They are consumers, not owners of the serialization model.

## Experimental seed

The repository contains an experimental JSON parser seed under `experimental/json_parser_seed/`. It originated as a VINOX Phase 4 implementation spike and is preserved here as research material. It is **not** the production JSON foundation of EQUORUS and is not approved for untrusted network input.

## Status

**Pre-v0.1 / architecture seed.** Public API and wire contracts are not frozen yet.

## License

Apache License 2.0. See [`LICENSE`](LICENSE).

---

Architecture seed and review lineage: **Mirko & Nephy 🔎**
