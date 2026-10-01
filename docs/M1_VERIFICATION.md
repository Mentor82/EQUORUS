# M1 verification — 2026-10-01

## Implemented scope

- C++20 owned values, immutable envelope snapshots and explicit type/version checks.
- Replaceable codec boundary; bounded ordinary JSON codec using nlohmann/json 3.12.0.
- Separate C++ pilot validators and independent Python reference.
- Offline schema fixtures, bidirectional object roundtrips and boundary rejection tests.
- C ABI ownership/error design and Windows/Linux hosted CI configuration.

The objects are EQUORUS-native. No consumer runtime is linked or deployed, no
LiNeP v0.2 wire format is changed, and no canonical/integrity result is claimed.

## Executed locally

Windows x64, CMake 3.29.2, Python 3.12.7:

| Compiler | Build | CTest |
| --- | --- | --- |
| MinGW GCC 13.2.0 | Debug / Ninja | 3/3 passed |
| MSVC 19.51.36257.0 | Debug / Ninja | 3/3 passed |

The conformance runner covers 86 shared cases, bidirectional roundtrips for every
accepted case, six native Python snapshots, ten adversarial cases and 648
deterministic type mutations. C++ tests also cover native construction, deep
ownership, uint64 boundaries, Unicode, embedded NUL, small binary64 values,
invalid exports and early abort of incomplete oversized/deep inputs.

Run commands: [BUILD_AND_API.md](BUILD_AND_API.md). Counts refer to assertions
inside the three CTest groups, not 86 independent CTest executables.

## VM108 and hosted CI

SSH verified 192.168.178.161 as `liara-dev`, Linux 7.2.5 x86_64. GCC 16.2.0,
CMake 3.31.6, Ninja, Clang and Python 3.14.7 are available; jsonschema imports.
An isolated directory was created: `/srv/liara/build/equorus-m1.Xt9JhOFs`.

Following explicit user authorization, the source/test archive was transferred
to that isolated directory and built without installing dependencies or services.

| VM108 configuration | Build | CTest |
| --- | --- | --- |
| GCC 16.2.0 | Debug / Ninja | 3/3 passed |
| GCC 16.2.0, AddressSanitizer + UndefinedBehaviorSanitizer | Debug / Ninja | 3/3 passed |

Both runs include all shared cases, 17 bidirectional roundtrips, six native
Python snapshots, ten adversarial cases and 648 type mutations. No sanitizer
diagnostics were reported. VM107 was not contacted.

The hosted workflow has been written but not pushed/run. Local Windows and VM108
Linux verification satisfy the M1 implementation checks; hosted CI execution is
still unverified. C ABI implementation, Go/Rust conformance, real consumer
adapters, fuzzing and release packaging remain later gates.
