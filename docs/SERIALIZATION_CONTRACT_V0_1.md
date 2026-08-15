# EQUORUS Minimal Common Serialization Contract v0.1

Status: **architecture draft**

This document captures the smallest common serialization contract derived from real requirements observed in LIARA, LiNeP, and VINOX.

## 1. Canonical object envelope

A serializable unit is modeled as:

```text
ObjectEnvelope
  type_id
  schema_version
  provenance
  payload
  canonical_profile? 
  integrity?
```

### Required fields

#### `type_id`

Stable semantic type identity, independent of implementation language or codec.

Examples:

```text
liara.heartbeat.snapshot
liara.memory.fact
linep.task
vinox.model.manifest
```

#### `schema_version`

Version of the semantic object contract, not the library version and not the transport protocol version.

Recommended form for v0.1:

```text
MAJOR.MINOR
```

Incompatible major versions fail closed unless an explicit migration path is selected.

#### `provenance`

Origin classification that survives serialization boundaries.

Initial provenance classes:

```text
SOURCE_LITERAL
TOOL_EVIDENCE
MODEL_GENERATED
DERIVED_CONTEXT
```

Serialization must preserve provenance. A semantic transformation produces a new derived object rather than silently changing the provenance of the source object.

#### `payload`

The semantic object content. The core contract does not prescribe its concrete encoding.

## 2. Serialization is not a codec

EQUORUS separates semantic object identity from encoding.

```text
Object -> Codec -> Bytes
Bytes  -> Codec -> Object
```

JSON, a compact binary encoding, or any future codec are interchangeable only when they preserve the object contract.

A consumer must not infer semantic type solely from a codec or MIME type.

## 3. Decode contract

Decode is always explicit about expectations:

```text
decode(
  bytes,
  expected_type,
  supported_schema_versions,
  resource_limits
) -> object | error
```

### Required behavior

- wrong `type_id` -> reject
- incompatible major schema version -> reject
- malformed encoding -> reject
- resource-limit violation -> reject
- ambiguous required-field interpretation -> reject

Minor-version forward compatibility is allowed only when the object contract defines additive evolution rules.

## 4. Canonicalization

Canonicalization is **optional and explicit**.

If deterministic bytes are required for hashing, signing, deduplication, audit comparison, or cross-implementation conformance, the object declares or is encoded with a named `canonical_profile`.

Examples:

```text
json-jcs-v1
linep-wire-v1
```

Ordinary encoding and canonical encoding are different operations.

## 5. Integrity

Integrity applies to a defined encoded representation.

```text
Object
  -> canonical encode
  -> bytes
  -> hash / CRC / MAC / signature
```

The base EQUORUS contract may transport integrity metadata, but does not itself decide which integrity mechanism a consumer must trust.

Transport-specific integrity remains transport-owned. For example, LiNeP header CRC/MAC semantics remain LiNeP responsibilities.

## 6. Resource limits

Every decoder must support explicit limits suitable to the codec and object type.

Minimum concepts:

```text
max_bytes
max_depth
max_items
max_string_length
```

Additional limits may be codec-specific.

Resource limits are part of the correctness and security contract, not optional tuning hints.

## 7. Ownership and lifetime

Language bindings and C ABI surfaces must define ownership for all returned buffers and strings.

No public API may return pointers whose validity changes due to unrelated internal container mutation unless that lifetime is explicitly controlled by the caller.

## 8. Non-goals

The v0.1 common contract does not define:

- network framing
- routing
- retries
- sequencing
- authorization
- governance decisions
- database persistence policy
- compression policy
- PKI/signature trust policy

These remain responsibilities of the relevant consumer or transport layer.

## 9. Consumer boundary examples

### LIARA

LIARA may serialize versioned service-contract objects, memory records, audit-relevant objects, and provenance-sensitive context. LIARA retains ownership of governance semantics and policy decisions.

### LiNeP

EQUORUS may encode/decode semantic payloads carried by LiNeP. LiNeP retains ownership of headers, framing, message IDs, sequence/correlation semantics, fragmentation, CRC/MAC and transport behavior.

### VINOX

VINOX may use EQUORUS for structured native payloads, provenance envelopes, manifests, tool schemas, and future MCP/HTTP representations. VINOX retains inference/model lifecycle ownership.

## 10. Conformance invariant

A round trip is conformant when the decoded object is contract-equivalent to the source object even if its in-memory layout or wire representation differs.

Core invariant:

> **Different representations. Equivalent information.**

Equivalence includes all fields declared invariant by the object schema, especially type identity, schema version, semantic payload, provenance, and required integrity metadata.

---

Draft lineage: derived from cross-project review of LIARA, LiNeP and VINOX requirements.

— Nephy 🔎
