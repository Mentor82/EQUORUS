"""Independent equorus-value-v1 canonical bytes and detached SHA-256 records."""
from dataclasses import dataclass
import hashlib
import hmac
import struct
from equorus_reference import Envelope, JsonCodec, Limits, check_value, fail

PROFILE = "equorus-value-v1"
ALGORITHM = "sha-256"
DOMAIN = b"EQUORUS-INTEGRITY\x00v1\x00"

def supported(profile, algorithm):
    if profile != PROFILE:
        fail("PROFILE")
    if algorithm != ALGORITHM:
        fail("ALGORITHM")

def canonical_bytes(value, profile=PROFILE, limits=Limits()):
    supported(profile, ALGORITHM)
    check_value(value, limits)
    result = bytearray()
    def append(data):
        if len(data) > limits.max_bytes - len(result):
            fail("LIMIT")
        result.extend(data)
    def count(n):
        append(n.to_bytes(8, "big"))
    def string(text):
        raw = text.encode("utf-8")
        append(b"s")
        count(len(raw))
        append(raw)
    def walk(v):
        if v is None:
            append(b"n")
        elif type(v) is bool:
            append(b"t" if v else b"f")
        elif type(v) in (int, float):
            append(b"d" + struct.pack(">d", v))
        elif isinstance(v, str):
            string(v)
        elif isinstance(v, list):
            append(b"a")
            count(len(v))
            for item in v:
                walk(item)
        else:
            append(b"o")
            count(len(v))
            for key in sorted(v, key=lambda s: s.encode("utf-8")):
                string(key)
                walk(v[key])
    walk(value)
    return bytes(result)

@dataclass(frozen=True)
class IntegrityRecord:
    profile: str
    algorithm: str
    digest: str

    def validate(self):
        supported(self.profile, self.algorithm)
        if (not isinstance(self.digest, str) or len(self.digest) != 64 or
                any(c not in "0123456789abcdef" for c in self.digest)):
            fail("INTEGRITY")

def compute_integrity(envelope: Envelope, profile=PROFILE, algorithm=ALGORITHM, limits=Limits()):
    supported(profile, algorithm)
    if not isinstance(envelope, Envelope):
        fail("TYPE")
    data = canonical_bytes(envelope.value, profile, limits)
    digest = hashlib.sha256(DOMAIN + profile.encode("ascii") + b"\x00" +
                            algorithm.encode("ascii") + b"\x00" + data).hexdigest()
    return IntegrityRecord(profile, algorithm, digest)

def verify_integrity(envelope, record, limits=Limits()):
    record.validate()
    actual = compute_integrity(envelope, record.profile, record.algorithm, limits)
    return hmac.compare_digest(actual.digest, record.digest)

def encode_integrity(record, limits=Limits()):
    record.validate()
    return JsonCodec().encode(dict(canonical_profile=record.profile,
                                  algorithm=record.algorithm, digest=record.digest), limits)

def decode_integrity(raw, limits=Limits()):
    value = JsonCodec().decode(raw, limits)
    if (not isinstance(value, dict) or set(value) != {"canonical_profile", "algorithm", "digest"}
            or any(not isinstance(v, str) for v in value.values())):
        fail("INTEGRITY")
    record = IntegrityRecord(value["canonical_profile"], value["algorithm"], value["digest"])
    record.validate()
    return record
