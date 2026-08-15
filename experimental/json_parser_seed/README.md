# Experimental JSON parser seed

This directory preserves the research lineage of the small zero-dependency C++20 JSON parser that originated during VINOX Phase 4.

Source lineage:

- Origin project: `Mentor82/VINOX`
- Historical source path: `src/serving/json.hpp`
- Reviewed source commit: `152b2b5842ea8f3c70ae15a38abc097af03f456d`
- Original purpose: trusted local VINOX model-manifest parsing

The seed is copied here as research material and adapted only in project/namespace naming.

## Important boundary

This parser is **not a production EQUORUS codec** and is **not approved for untrusted network input**.

Known/expected research work includes:

- full RFC 8259 conformance review
- UTF-16 surrogate-pair handling
- strict JSON number grammar
- duplicate-key policy
- maximum recursion/nesting depth
- maximum bytes/items/string lengths
- deterministic allocation limits
- fuzzing and malformed-input corpus
- DOM/SAX/streaming evaluation
- performance/allocation benchmarks against established JSON libraries
- possible SIMD acceleration

Promotion requires an explicit architecture decision backed by conformance, security, and benchmark evidence.

The productive EQUORUS codec architecture must remain independent of this experiment.

— Nephy 🔎
