# Consumer contracts — pilot v0.1

Status: accepted implementation baseline; not a frozen wire/API contract.
Date: 2026-10-01. All example names, IDs and timestamps are synthetic.

## Sources and scope

These are pinned design inputs, not claims about deployed binaries or current
remote branch heads. Recheck a consumer revision before implementing its adapter.

| Consumer | Source revision and contract |
| --- | --- |
| VINOX | [1a289704 — vinox_provenance_meta](https://github.com/Mentor82/VINOX/blob/1a289704d2a3b846f405b581bcb676c989bad3d0/include/vinox/vinox.h) |
| LIARA | [a886360e — HeartbeatSnapshot / ResourceObservation](https://github.com/Mentor82/L.I.A.R.A./blob/a886360e2e29c8f9f89793c2e1c2eea60b113bdf/services/contracts/heartbeat.py) |
| LiNeP v0.2 | [caa2b2f4 — request_envelope / generation_options](https://github.com/Mentor82/LiNeP/blob/caa2b2f4ed2b6d5d0ec064e055b6065143845dfd/LiNeP/include/linep/v0_2/envelopes.hpp) |
| LiNeP-Ollama | [075c1dab — Go RequestEnvelope / GenerationOptions](https://github.com/Mentor82/LiNeP-Ollama/blob/075c1dab0ce6513fdaedea384cb4d7e4bae42807/pkg/linep/v02/types.go) |
| L.I.A.R.A.-OS | [245b3a05 — Rust LiNeP v0.2 codec](https://github.com/Mentor82/L.I.A.R.A.-OS/blob/245b3a0534ea1d6a444de27d594a5a70533934b8/src/liara-noded/src/linep_v02.rs) |

L.I.A.R.A.-Cluster / Cluster OS is excluded. The user identified
**192.168.178.161 -> VM108 -> L.I.A.R.A. OS DEV** on 2026-10-01 as the development
target for native builds/integration. During M1 the host was verified over SSH as
`liara-dev`, Linux 7.2.5, with GCC 16.2.0 and Python 3.14.7. Build/test execution is
recorded separately in [M1_VERIFICATION.md](M1_VERIFICATION.md).
VM107 is not a target of this work package.

## Shared pilot representation

The three JSON Schemas in `schemas/pilot-v0.1/` describe ordinary UTF-8 JSON
projections. Every envelope requires `type_id`, `schema_version`, `provenance`
and `payload`. Envelope `schema_version` is exactly `0.1`; this is independent
of LIARA's payload version `1.0` and LiNeP's protocol version `0.2`.

- Required means present: null is accepted only where a schema explicitly allows it.
  Decoding never inserts defaults. Producers materialize consumer defaults before
  projection; wire presence flags such as `has_options` must survive independently.
- Pilot structural objects reject unknown fields and versions, including unknown
  minor versions. Future additive compatibility requires an explicit revision and
  tests. No generic schema registry or runtime schema download is needed.
- `provenance.kind` uses the four existing classes. Optional `source_id` may be
  absent, null or an empty string; these are distinct. Optional `timestamp_ms`
  is a uint64 decimal string. Missing source metadata is not fabricated.
  An adapter needs caller-supplied provenance if its source lacks it.
- Provenance records origin, not truth, trust or authorization. Evidence status,
  confidence and user ownership remain consumer fields. Unknown provenance classes
  fail closed in this pilot. Mixed/field-level lineage is deferred, not flattened.
- uint64 fields are decimal JSON **strings**, range 0..18446744073709551615,
  no sign, whitespace or leading zeroes except `"0"`. Request/execution IDs must
  be nonzero. Never route these values through a floating-point number.
- Other JSON numbers are finite binary64 values, with exactly represented integer
  values restricted to +/-9007199254740991 unless a narrower schema range applies.
  Negative zero and nonzero numeric tokens underflowing binary64 are rejected.
  Unsupported values must be rejected by
  exporters, not rounded or replaced. General decimal/binary types are deferred.
- LiNeP float32 fields use JSON numbers equal to the exact binary32 value widened
  to binary64. For example float32 `0.9` projects as `0.8999999761581421`.
  This proposed strict mapping avoids silent narrowing. Native adapters must test
  the bit-preserving roundtrip; the fixture checker only checks representability.
- Strings preserve Unicode scalar values without normalization. Duplicate keys,
  lone surrogates, invalid UTF-8, BOM, NaN/Infinity and trailing data are rejected.
  Array order matters; object member order does not. Schema string lengths count
  characters; byte limits below also apply to decoded keys and string values.
- Timestamp strings retain their supplied timezone/precision (up to microseconds,
  matching the pilot Python datetime mapping). No implicit rounding, local timezone
  conversion or replacement with the current time. VINOX milliseconds stay integers.
- Inline `canonical_profile` and `integrity` are unsupported and rejected.
  M2 adds a [detached record](CANONICAL_INTEGRITY_V1.md) without changing this schema.
  Pretty-printed fixtures are not canonical bytes and carry no hash/signature claim.

## Resource budget

Pilot defaults: `max_bytes=65536`, `max_depth=12`, `max_items=2048`,
`max_string_length=8192` UTF-8 bytes. Tests may supply smaller budgets.
Root depth is 1, every child value adds one; each value/container counts as one
item, keys do not count as items but their UTF-8 bytes are string-limited.
`max_bytes` counts the complete input including whitespace. Limits apply to the
whole envelope. A consumer's stricter limit still applies; this is not a change
to LiNeP v0.2's transport payload limit.

The local fixture checker parses small trusted repository fixtures and checks
depth/items afterward. It is **not** an allocation-safe network decoder.
The M1 codecs enforce structural/string budgets before building their value
trees. Exact allocator-byte accounting and fuzz campaigns remain release work;
see [BUILD_AND_API.md](BUILD_AND_API.md).

## VINOX provenance snapshot

Type: `vinox.provenance.snapshot`; payload is `{}`. The snapshot's subject is
the metadata itself, not an arbitrary VINOX request. All represented source
metadata lives in `provenance`, avoiding two conflicting copies.

| Native field | Projection | Invariant |
| --- | --- | --- |
| kind 0/1/2/3 | SOURCE_LITERAL / TOOL_EVIDENCE / MODEL_GENERATED / DERIVED_CONTEXT | Exact mapping; unknown rejected |
| source_id | source_id | Preserve null pointer as null, empty string as empty; omitted only if ABI field unavailable |
| timestamp_ms | timestamp_ms decimal string | Exact uint64, zero is preserved |
| struct_size | Not serialized | ABI boundary checks field availability; never serialize pointers or padding |

Tests cover full metadata and minimum available metadata. Reverse conversion
requires an ABI size consistent with available fields and owned backing strings;
it must not synthesize a missing timestamp. M1 tests native envelope ownership;
exported C ABI lifetime tests are required when that ABI is implemented.

## LIARA heartbeat snapshot

Type: `liara.heartbeat.snapshot`. Payload projects the existing snapshot fields:
`schema_version`, `instance_id`, `instance_type`, `node_id`, `sequence`,
`observed_at`, `state`, `observations`, `signals`, `confidence`.

`sequence` becomes a uint64 decimal string. Python sequences outside uint64 are
rejected by the adapter. All ResourceObservation fields are materialized and
preserved. Numeric observations/confidence follow the shared numeric rules.
Metric/unit, ratio, count and boolean-value constraints mirror the pinned source.
The pilot adds finite-value, explicit-timezone and resource restrictions.

For this first fixture subset `attributes` must be empty. Typed arbitrary
attributes (especially Python int vs float vs bool) need a reviewed mapping;
silently dropping them is forbidden. This deliberately does not claim support
for every valid upstream HeartbeatSnapshot. Empty observations remain valid as in
the snapshot source (do not apply ObservationBatch's non-empty rule here).

## LiNeP v0.2 request projection

Type: `linep.v02.request`. Payload contains explicit `protocol_version: "0.2"`,
`stream`, `profile`, `model_id`, `payload`, `max_tokens`, `temperature`,
`stream_requested`, `has_options`, and conditionally `options`.

- Stream request/execution IDs use uint64 strings; output_id and max_tokens use
  uint32 numbers. Profile maps 1/2/3 to generate/chat/embed; unknown fails closed.
- Request payload remains an opaque string, including structured chat JSON. Do
  not parse/reformat it or flatten its roles. Empty payload is preserved.
- `has_options=false` requires options to be absent. `has_options=true` requires
  all option fields, even when their values equal the source defaults. Unused
  in-memory option values when the flag is false are not wire semantics.
- Seed is uint64 text; top_k/repeat_last_n are int32 numbers. Floating-point
  options follow the exact float32 projection rule above. Sampling policy stays
  with the runtime rather than inventing new top_p/penalty ranges here.
- stop_sequences retain order. extra_options use a sorted array of `[key,value]`
  pairs with unique keys, ordered by UTF-8 bytes, matching canonical option order.
  Reject duplicates and unsorted projections. Compare native options after the
  LiNeP-defined canonical ordering, not incidental vector insertion order.

This JSON is an exchange/test projection, **not** a LiNeP v0.2 frame or something
to feed directly into an existing CHAT payload. The future adapter maps back to
the native request and then uses the existing LiNeP v0.2 encoder. Header/lease,
SESSION_BIND, MAC and LiNeP-SL handling remain outside EQUORUS.

## Validation and outstanding gates

Run the command in [tests/README.md](../tests/README.md). JSON Schema validates
shape; the fixture checker additionally checks uint64 range, exact float32,
metric/unit relationships, timestamps, numeric/Unicode rules and budgets.
`cases.json` defines deterministic single mutations and expected error categories.

M1 implements native ownership and C++/Python roundtrips; M2 defines and tests
canonical bytes and detached integrity. Before freezing: review the restrictive
numeric/attribute subset and extend conformance to Go/Rust.
Binary blobs, mixed provenance, general typed
attributes and unknown-field preservation require explicit extensions.
