# C ABI design — M1 draft

This is a design contract, not an exported header/library ABI. Implement and test
it before asking consumers to link against EQUORUS. Freeze numeric status values
and layouts with ABI smoke tests at that point.

## Proposed surface

- Decode: input pointer/byte length, expected type pointer/length, supported
  versions and limits -> status and opaque immutable envelope handle.
- Encode: envelope handle and codec/profile -> status and owned buffer handle.
- Envelope/buffer release functions; null release is a no-op.
- Explicit access/copy functions; no JSON DOM types, C++ containers, references,
  exceptions or allocator types cross the ABI.

Lengths use explicitly sized unsigned integers. `struct_size` and a separate
`abi_version` precede extensible options. Check field availability before access;
absent fields are not read. Reject unknown required features. ABI, EQUORUS schema,
codec profile and LiNeP v0.2 versions are independent.

## Ownership and failure

Input pointers are borrowed only during a call. Successful decode owns every
referenced byte before returning. Buffer pointers last until buffer release,
even if their originating envelope is released. Callers never free library
storage through a different allocator.

Output handles are nulled before work and remain null on failure. Catch all C++
exceptions at the ABI boundary: allocation failure becomes out-of-memory status;
unexpected exceptions become internal-error status. Publish no partial handle.
Errors are per call, never borrowed thread-local pointers. Diagnostics use
caller-provided storage plus capacity/required length, without an implicit NUL
termination assumption.

Immutable handles allow concurrent reads; release requires external synchronization.
Reference counting, if introduced, must be explicit. External validator callbacks
require a separate ownership/exception contract before ABI exposure.

## Required implementation tests

C-only compile/link, null/zero-length distinctions, all error exits, independent
allocator boundaries, release order, embedded NULs, short option structures,
unsupported ABI versions, concurrent reads and sanitizer lifetime checks.
No C ABI support is advertised before those tests pass.
