# Codecs

Codec implementations belong here.

A codec converts between EQUORUS semantic objects and bytes. Codec choice must not define object identity, schema version, provenance semantics, or consumer policy.

Planned initial production candidate: JSON.

Experimental parser research is kept separately under `experimental/` until it satisfies conformance, fuzzing, resource-limit, and architecture-review requirements.
