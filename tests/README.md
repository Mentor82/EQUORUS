# Tests

EQUORUS tests should emphasize **contract equivalence**, not implementation identity.

Planned categories:

- object/schema validation
- schema-version evolution
- provenance round trips
- canonical byte fixtures
- malformed-input rejection
- decode resource limits
- ownership/lifetime tests for C ABI surfaces
- cross-language golden fixtures (C++/Python)
- consumer-boundary fixtures for LIARA, LiNeP, and VINOX
- fuzzing for production codecs
