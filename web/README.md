# EQUORUS Studio (v0.1)

EQUORUS Studio is a modular, zero-dependency browser workbench for inspecting, validating, and testing EQUORUS envelopes, canonical byte streams (`equorus-value-v1`), detached SHA-256 integrity records, consumer adapters (VINOX, L.I.A.R.A., LiNeP), and polyglot code generation.

## Features

1. **Envelope Inspector & Validator (`js/core/validator.js`)**:
   - Live checks for duplicate keys, lone surrogates, forbidden numbers (`-0.0`, `NaN`), and numeric safe integer bounds.
   - Domain invariants for VINOX provenance, LIARA heartbeat (strict `{}` attributes), and LiNeP request.
2. **Canonical Serializer (`js/core/canonical.js`)**:
   - Injective `equorus-value-v1` byte grammar serializer.
   - Formatted hex dump and raw vs canonical byte comparison.
3. **Detached Integrity Engine (`js/core/integrity.js`)**:
   - Live SHA-256 computation over domain-separated preimage (`EQUORUS-INTEGRITY\0v1\0equorus-value-v1\0sha-256\0` + bytes).
   - Interactive Tamper Simulator (bit-flip detector).
4. **Consumer Adapter Visualizers (`js/adapters/`)**:
   - VINOX: provenance kind, source ID, timestamp.
   - LIARA: sequence, health status, observation table, empty attributes invariant.
   - LiNeP v0.2: profile, model, streaming options, canonical extra-options key sorting.
5. **Polyglot Code Generator (`js/codegen/`)**:
   - Instant code snippets for C++20, C ABI, Python, Go, and Rust.

## How to Run

Because the frontend uses standard ES modules (`<script type="module">`), modern browsers require it to be served via a local web server (to satisfy CORS origin policies for file imports):

### Using Python (built-in):
```bash
python -m http.server 8000 --directory web
```
Then open `http://localhost:8000` in your browser.

### Using Node / npx:
```bash
npx serve web
```

Or view directly as an Antigravity Generative UI artifact!
