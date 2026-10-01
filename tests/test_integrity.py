"""Frozen canonical vectors, independent implementations and corruption checks."""
import copy
import hashlib
import json
import math
from pathlib import Path
import random
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))
from equorus_reference import ContractError, Envelope, JsonCodec, Limits
from equorus_integrity import (ALGORITHM, DOMAIN, PROFILE, IntegrityRecord,
    canonical_bytes, compute_integrity, decode_integrity, encode_integrity, verify_integrity)

CLI = sys.argv[1]
codec = JsonCodec()

def cpp(mode, arg, raw, limits=None):
    command = [CLI, mode, arg]
    if limits is not None:
        command += [str(v) for v in vars(limits).values()]
    result = subprocess.run(command, input=raw, capture_output=True, timeout=20)
    assert result.returncode in (0, 1), (command, result.stderr)
    return result.returncode, result.stdout

def ok(mode, arg, raw, limits=None):
    status, output = cpp(mode, arg, raw, limits)
    assert status == 0, (mode, output)
    return output

def rejects(code, f):
    try:
        f()
    except ContractError as e:
        assert str(e) == code, (code, str(e))
    else:
        raise AssertionError(f"expected {code}")

vectors = json.loads((ROOT / "tests/fixtures/canonical-v1/vectors.json").read_text(encoding="utf-8"))
assert vectors["profile"] == PROFILE
for vector in vectors["values"]:
    raw = vector["json"].encode("utf-8")
    expected = bytes.fromhex(vector["hex"])
    assert canonical_bytes(codec.decode(raw)) == expected, vector["name"]
    assert ok("canonical", PROFILE, raw) == expected, vector["name"]

corruptions = 0
for vector in vectors["envelopes"]:
    raw = (ROOT / "tests/fixtures/pilot-v0.1" / vector["file"]).read_bytes()
    value = codec.decode(raw)
    expected = bytes.fromhex(vector["hex"])
    env = Envelope(value, value["type_id"])
    record = IntegrityRecord(PROFILE, ALGORITHM, vector["digest"])
    assert canonical_bytes(env.value) == expected
    assert ok("canonical", PROFILE, raw) == expected
    assert compute_integrity(env) == record
    assert decode_integrity(ok("hash", env.value["type_id"], raw)) == record
    assert verify_integrity(env, record)
    assert decode_integrity(ok("record", "unused", encode_integrity(record))) == record
    record_value = json.loads(encode_integrity(record))
    request = dict(envelope=value, integrity=record_value)
    assert ok("verify", value["type_id"], codec.encode(request)) == b"true"

    # Whitespace, key insertion order and equivalent numeric spellings don't matter.
    reordered = json.dumps(value, ensure_ascii=True, sort_keys=True, indent=3).encode()
    assert compute_integrity(Envelope.decode(reordered, value["type_id"])) == record
    assert decode_integrity(ok("hash", value["type_id"], reordered)) == record

    # Mutate every scalar field (including type, schema, provenance and payload).
    # Schema-invalid changes must fail validation, never silently verify.
    def leaves(v, path=()):
        if isinstance(v, dict):
            for k, x in v.items():
                yield from leaves(x, path + (k,))
        elif isinstance(v, list):
            for i, x in enumerate(v):
                yield from leaves(x, path + (i,))
        else:
            yield path, v
    for path, old in leaves(value):
        mutated = copy.deepcopy(value)
        target = mutated
        for key in path[:-1]:
            target = target[key]
        target[path[-1]] = (not old if type(old) is bool else
                           old + 1 if type(old) in (int, float) else
                           "changed" if old is None else old + "x")
        try:
            candidate = Envelope(mutated, value["type_id"])
        except ContractError:
            pass
        else:
            assert not verify_integrity(candidate, record), path
        status, output = cpp("verify", value["type_id"], codec.encode(dict(envelope=mutated, integrity=record_value)))
        assert (status == 0 and output == b"false") or (status == 1 and output.startswith(b"ERROR ")), path
        corruptions += 1

    for index in range(64):
        bad = record.digest[:index] + ("1" if record.digest[index] == "0" else "0") + record.digest[index+1:]
        altered = IntegrityRecord(PROFILE, ALGORITHM, bad)
        assert not verify_integrity(env, altered)
        assert ok("verify", value["type_id"], codec.encode(dict(envelope=value, integrity=json.loads(encode_integrity(altered))))) == b"false"
        corruptions += 1

    # Domain separation differs from hashing just the canonical bytes.
    assert record.digest != hashlib.sha256(expected).hexdigest()

# Random binary64 bit patterns, including subnormals, checked in a single batch.
rng = random.Random(20261001)
numbers = [0.0, 5e-324, 2.2250738585072014e-308, 0.1, -0.1, 9007199254740991.0]
while len(numbers) < 1000:
    n = struct.unpack(">d", rng.getrandbits(64).to_bytes(8, "big"))[0]
    if math.isfinite(n) and not (n == 0 and math.copysign(1, n) < 0) and not (n.is_integer() and abs(n) > 9007199254740991):
        numbers.append(n)
raw = codec.encode(numbers)
assert ok("canonical", PROFILE, raw) == canonical_bytes(numbers)

for left, right in [(b"1", b"1.0"), (b"0.1", b"1e-1"), (b'"a"', b'"\\u0061"')]:
    assert ok("canonical", PROFILE, left) == ok("canonical", PROFILE, right)
assert canonical_bytes(None) != canonical_bytes("")
assert canonical_bytes(False) != canonical_bytes(0)
assert canonical_bytes({}) != canonical_bytes({"x": None})
assert canonical_bytes("\u00e9") != canonical_bytes("e\u0301")
assert canonical_bytes([1,2]) != canonical_bytes([2,1])
for raw, code in [(b"-0", "NUMBER"), (b"1e-400", "NUMBER"), (b"NaN", "NUMBER"),
                  (b'"\\ud800"', "UNICODE"), (b'{"a":1,"\\u0061":2}', "DUPLICATE_KEY")]:
    rejects(code, lambda: canonical_bytes(codec.decode(raw)))
    assert cpp("canonical", PROFILE, raw) == (1, f"ERROR {code}".encode())
rejects("PROFILE", lambda: canonical_bytes(None, "json-jcs-v1"))
assert cpp("canonical", "json-jcs-v1", b"null") == (1, b"ERROR PROFILE")
for raw, limits in [(b"[]", Limits(max_bytes=8)), (b"[0]", Limits(max_depth=1)),
                    (b"[0]", Limits(max_items=1)), (b'"abc"', Limits(max_string_length=2))]:
    rejects("LIMIT", lambda: canonical_bytes(codec.decode(raw, limits), limits=limits))
    assert cpp("canonical", PROFILE, raw, limits) == (1, b"ERROR LIMIT")
assert canonical_bytes([], limits=Limits(max_bytes=9)) == ok("canonical", PROFILE, b"[]", Limits(max_bytes=9))

baseline = dict(canonical_profile=PROFILE, algorithm=ALGORITHM, digest="0"*64)
for key, new, code in [("canonical_profile", "unknown", "PROFILE"), ("algorithm", "sha-1", "ALGORITHM"),
                       ("digest", "F"*64, "INTEGRITY"), ("digest", "0"*63, "INTEGRITY"),
                       ("digest", None, "INTEGRITY")]:
    bad = dict(baseline, **{key:new})
    raw = codec.encode(bad)
    rejects(code, lambda: decode_integrity(raw))
    assert cpp("record", "unused", raw) == (1, f"ERROR {code}".encode())
for bad in [[], {}, dict(baseline, extra=True), {k:v for k,v in baseline.items() if k != "algorithm"}]:
    raw = codec.encode(bad)
    rejects("INTEGRITY", lambda: decode_integrity(raw))
    assert cpp("record", "unused", raw) == (1, b"ERROR INTEGRITY")

print(f"PASS: {len(vectors['values'])} canonical vectors; {len(vectors['envelopes'])} envelope goldens; "
      f"{corruptions} corruption checks; 1000 binary64 values; metadata and limit rejections")
