# Tests

EQUORUS tests should emphasize **contract equivalence**, not implementation identity.

## M1 native and cross-language tests

See [build/API](../docs/BUILD_AND_API.md). CTest runs C++ ownership/codec tests,
the independent C++/Python conformance runner and the original fixture checker.
The conformance runner compares both directions and native snapshots, and includes
deterministic structural mutations and malformed/budget cases. These tests use
EQUORUS objects; actual consumer runtimes and LiNeP v0.2 wire frames are not run.

## Executable pilot fixtures

M2 adds two CTest groups: native integrity tests and independent C++/Python byte
and digest comparisons. Frozen vectors are in `fixtures/canonical-v1/vectors.json`.
The suite tests 17 value vectors, six complete envelopes, 460 corruption cases,
1,000 binary64 values, equivalent JSON encodings and invalid metadata/budgets.
See the [normative profile](../docs/CANONICAL_INTEGRITY_V1.md).

From the repository root, with Python 3.10+ and `jsonschema` 4.x installed:

```text
python tests/validate_contract_fixtures.py
```

If needed, install the development dependency with
`python -m pip install -r tests/requirements.txt` in your development environment.
Validation itself is offline and writes no files.

The checker validates three draft JSON Schemas and the accepted/rejected cases
in `fixtures/pilot-v0.1/cases.json`. Cases either use a complete example, apply
explicit field mutations, or supply raw malformed input. Error categories are
fixture expectations, not a frozen public API. See
[consumer contracts](../docs/CONSUMER_CONTRACTS_V0_1.md) for semantic rules.

Examples are synthetic. Their formatting is not a canonical byte format.
The checker is not a production decoder: depth/item limits are checked after
parsing trusted small fixtures. It does not execute native C++/Python consumer
roundtrips, Go/Rust code, LiNeP wire interoperability or integrity verification.

Coverage through M2 and later planned categories:

- object/schema validation
- schema-version evolution
- provenance round trips
- canonical byte fixtures
- malformed-input rejection
- decode resource limits
- ownership/lifetime tests for C ABI surfaces
- cross-language golden fixtures (C++/Python, then Go/Rust)
- consumer-boundary fixtures for LIARA, LiNeP v0.2, VINOX and L.I.A.R.A.-OS
- fuzzing for production codecs
