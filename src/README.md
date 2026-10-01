# Core source

M1 owned values and immutable envelopes live here. Consumer-specific pilot
validation is isolated in `pilot.cpp`; it does not enter the generic core target.

The core must remain independent of LIARA, LiNeP, and VINOX domain semantics. Consumer-specific integration belongs in adapters/fixtures, not in the serialization kernel.
