"""EQUORUS - Lossless Canonical Object Envelope & Conformance Engine."""

try:
    from .equorus_reference import Envelope, Limits, ContractError, JsonCodec
    from .equorus_integrity import (
        compute_integrity,
        verify_integrity,
        canonical_bytes,
        PROFILE,
        ALGORITHM,
    )
except ImportError:
    from equorus_reference import Envelope, Limits, ContractError, JsonCodec
    from equorus_integrity import (
        compute_integrity,
        verify_integrity,
        canonical_bytes,
        PROFILE,
        ALGORITHM,
    )

__all__ = [
    "Envelope",
    "Limits",
    "ContractError",
    "JsonCodec",
    "compute_integrity",
    "verify_integrity",
    "canonical_bytes",
    "PROFILE",
    "ALGORITHM",
]
