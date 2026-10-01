# M2 verification — 2026-10-01

Implemented: [canonical value bytes and detached integrity](CANONICAL_INTEGRITY_V1.md).
The M1 envelope and LiNeP v0.2 projection schemas are unchanged.

## Executed builds

| Target | Configuration | Result |
| --- | --- | --- |
| Windows x64, MinGW GCC 13.2.0, Python 3.12.7 | Debug / Ninja | 5/5 CTest groups passed |
| Windows x64, MSVC 19.51.36257.0, Python 3.12.7 | Debug / Ninja | 5/5 CTest groups passed |
| VM108, GCC 16.2.0, Python 3.14.7 | Debug / Ninja, ASan + UBSan | 5/5 CTest groups passed; no sanitizer diagnostics |

VM108 source/build directory: `/srv/liara/build/equorus-m1.Xt9JhOFs/m2`.
Archive SHA-256: `f3a561d26ef0ebf447643cd0b7db61c051f3c83506b6525853a6dc48a0da6c68`.
Only source, tests, schemas and public vendored dependencies were transferred.
No service was installed or deployed. VM107 was not contacted.

## Coverage

- All M1 checks: 86 shared cases, 17 bidirectional roundtrips, six native Python
  snapshots, ten adversarial cases and 648 type mutations.
- 17 frozen canonical value vectors and six complete-envelope byte/digest goldens.
- 460 field/digest corruption checks, including provenance, payload and identity.
- 1,000 deterministic finite binary64 values, including subnormals.
- Equivalent JSON spellings, UTF-8 key ordering, embedded NUL, null/absence,
  array order, composed/decomposed Unicode and bool/number distinction.
- Unsupported profile/algorithm, malformed records, duplicate keys, invalid
  numbers/Unicode and byte/depth/item/string budget rejection.
- SHA-256 known answers for empty input, `abc` and one million `a` bytes.

## Hosted CI

The [implementation run](https://github.com/Mentor82/EQUORUS/actions/runs/36920840172)
for commit `db9d41d97c1711d34a2a903faa7ffef1714a7ac3` passed all five test groups
on both Ubuntu (GCC 13.3.0) and Windows (MSVC 19.51.36260.0), from fresh checkouts.
Both jobs used Python 3.12. The workflow now uses current checkout/setup-python
releases to address the Node.js 20 deprecation annotation in that first run.
Subsequent branch pushes rerun all five groups automatically; inspect the
[branch runs](https://github.com/Mentor82/EQUORUS/actions?query=branch%3Acodex%2Fm2-canonical-integrity)
for the latest commit-specific result.

The C ABI, Go/Rust, consumer integration, fuzzing and release packaging remain
separate milestones.
