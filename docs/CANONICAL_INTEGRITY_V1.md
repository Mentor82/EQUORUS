# M2 canonical bytes and detached integrity

Status: implemented experimental profile; public API is not frozen.

## Profile decision

`equorus-value-v1` defines an injective byte representation of the supported
EQUORUS value tree for hashing and comparison. It is separate from ordinary
JSON encoding. It is not a binary transport codec and has no public decoder.

We reviewed [JCS / RFC 8785](https://www.rfc-editor.org/rfc/rfc8785.html).
JCS requires ECMAScript number formatting and UTF-16 property ordering. The
existing C++ and Python JSON encoders do not establish those guarantees.
M2 therefore uses explicitly tagged values with exact binary64 bits and UTF-8
ordering. It does **not** claim JCS compatibility. A future JCS implementation
must have a distinct identifier and its own conformance suite.

This choice preserves the pilot's numeric and Unicode model without depending
on decimal output algorithms, host byte order, object layout or locale.

## Normative byte grammar

All tags below are single ASCII bytes. `U64(n)` is exactly eight bytes of
unsigned big-endian integer. There is no whitespace, padding, terminator or BOM.

| Value | Canonical bytes |
| --- | --- |
| null | ASCII `n` |
| false / true | ASCII `f` / `t` |
| number | ASCII `d`, then eight bytes of IEEE-754 binary64 bits, most significant byte first |
| string | ASCII `s`, U64(UTF-8 byte length), exact UTF-8 bytes |
| array | ASCII `a`, U64(element count), each canonical element in array order |
| object | ASCII `o`, U64(member count), canonical string key then canonical value for each member |

Object keys are sorted lexicographically by **unsigned UTF-8 bytes**, including
embedded NUL. A shorter equal prefix sorts first. This is not UTF-16 ordering.
Strings must be valid Unicode scalar sequences; no normalization or replacement
is performed. Thus composed and decomposed text may differ. JSON escape choices
and object insertion order disappear after parsing; array order remains relevant.

Numbers follow the M1 pilot: finite binary64, no negative zero, and integer-valued
numbers within +/- (2^53 - 1). Native Python integers in that range are converted
exactly to binary64. `1`, `1.0` and `1e0` represent the same number; boolean true
and numeric one have different tags. Subnormals are preserved. uint64 domain
values remain validated decimal strings, including values above 2^53 - 1.

Examples, hexadecimal:

- null: `6e`
- true: `74`
- numeric one: `643ff0000000000000`
- empty string: `730000000000000000`
- empty object: `6f0000000000000000`

Frozen scalar/nested and full-envelope vectors are published in
[`vectors.json`](../tests/fixtures/canonical-v1/vectors.json). Tests read these
bytes; they do not regenerate expected results during execution.

## SHA-256 integrity contract

Algorithm identifier: `sha-256`. Records are **detached** JSON objects containing
exactly these three string fields:

```json
{"canonical_profile":"equorus-value-v1","algorithm":"sha-256","digest":"<64 lowercase hex digits>"}
```

The digest is SHA-256 of the following concatenation:

```text
ASCII("EQUORUS-INTEGRITY") || 0x00 || ASCII("v1") || 0x00
|| ASCII("equorus-value-v1") || 0x00 || ASCII("sha-256") || 0x00
|| canonical_bytes(complete validated envelope)
```

The entire `type_id`, `schema_version`, `provenance` and `payload` are covered,
including absence versus null, array order and every nested field. The detached
digest itself is excluded. Profile and algorithm are bound into the preimage;
there is no fallback or automatic algorithm negotiation. Unknown identifiers
raise `PROFILE` / `ALGORITHM`. Malformed record structure/digest raises
`INTEGRITY`; malformed JSON and resource errors retain their existing codes.
A valid record whose digest differs returns false. An invalid envelope fails
type/schema validation before it can be used as an integrity-verified object.

The `canonical_profile` and `integrity` fields inside the pilot envelope remain
unsupported: M2 adds a detached record, preserving the four-field M0/M1 contract.
Record JSON formatting is ordinary JSON; callers compare decoded record fields,
not serialized record bytes. Compare canonical bytes when byte equality matters.

This unkeyed checksum detects changes against a trusted expected digest. Anyone
who can replace both content and digest can recompute it. It is not a MAC,
signature, authentication, replay defense or authorization decision. Existing
LiNeP v0.2 / LiNeP-SL transport integrity and L.I.A.R.A.-OS signatures are unchanged.

## APIs and bounds

C++: include `equorus/integrity.hpp`, link `equorus::integrity`.

```cpp
auto envelope = equorus::pilot::decode(json, "linep.v02.request");
auto record = equorus::compute_integrity(envelope);
auto metadata = equorus::encode_integrity(record);
bool matches = equorus::verify_integrity(envelope, equorus::decode_integrity(metadata));
```

Python: `equorus_integrity` exposes the equivalent operations; it uses its own
canonical writer and Python hashlib, not C++ bindings. `compute_integrity`
requires an already validated `Envelope`. `canonical_bytes(Value)` is a lower
level structural API: it enforces the value model but does not assert a domain
schema or type identity.

`Limits` applies independently to each operation. Canonical `max_bytes` limits
the returned canonical byte sequence, not JSON length. Length tags can make the
canonical representation larger than JSON; a previously accepted JSON document
may therefore exceed the canonical limit. The writer checks before each append.
Tree depth, items and decoded string lengths retain M1 semantics. Keys count
toward string limits, not item counts. The hash adds only its fixed prefix beyond
the bounded canonical bytes. Metadata decode/encode also enforce their limits.

The C++ SHA-256 dependency is private, pinned PicoSHA2; CMake verifies its source
hash. Known-answer tests cover empty input, `abc` and one million `a` bytes.
Cross-language tests compare published vectors, actual envelope digests, numeric
bit patterns, corruption, equivalent JSON representations and rejection behavior.

No cryptographic certification, signature support, fuzzing completion or public
C ABI is implied by M2.
