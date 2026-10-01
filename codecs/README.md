# Codecs

Codec implementations belong here.

A codec converts between EQUORUS semantic objects and bytes. Codec choice must not define object identity, schema version, provenance semantics, or consumer policy.

M1 implements ordinary JSON in `json.cpp`, using a vendored nlohmann/json SAX
parser behind allocation-free budget preflight and a bounded encoder. Public
value/envelope headers do not expose the dependency. Canonical JSON and integrity
are not implemented. See [build/API](../docs/BUILD_AND_API.md).

Experimental parser research is kept separately under `experimental/` until it satisfies conformance, fuzzing, resource-limit, and architecture-review requirements.
