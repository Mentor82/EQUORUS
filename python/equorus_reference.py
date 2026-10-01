"""Independent bounded Python reference for the pilot contract (development API).

Uses Python's JSON string decoder and JSON Schema, never C++ bindings or the
offline fixture checker. Schemas are local repository resources.
"""
from __future__ import annotations
from dataclasses import dataclass
from datetime import datetime
import json
import math
from pathlib import Path
import re
import struct
from types import MappingProxyType

from jsonschema import Draft202012Validator

class ContractError(ValueError):
    pass

def fail(code):
    raise ContractError(code)

@dataclass(frozen=True)
class Limits:
    max_bytes: int = 65536
    max_depth: int = 12
    max_items: int = 2048
    max_string_length: int = 8192

    def check(self):
        if any(type(v) is not int or v < 0 for v in vars(self).values()) or self.max_depth > 128:
            fail("LIMIT")

NUMBER = re.compile(r"-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?")
UINT = re.compile(r"(?:0|[1-9][0-9]{0,19})", re.ASCII)
TIME = re.compile(r"[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}(?:\.[0-9]{1,6})?(?:Z|[+-][0-9]{2}:[0-9]{2})")
SAFE_INT = 9007199254740991

def numeric(n):
    if type(n) not in (float, int):
        fail("SCHEMA")
    if type(n) is int and abs(n) > SAFE_INT:
        fail("NUMBER")
    if not math.isfinite(n) or (n == 0 and math.copysign(1, n) < 0):
        fail("NUMBER")
    if (type(n) is int or n.is_integer()) and abs(n) > SAFE_INT:
        fail("NUMBER")

class Parser:
    def __init__(self, raw, limits):
        limits.check()
        if len(raw) > limits.max_bytes:
            fail("LIMIT")
        try:
            self.text = raw.decode("utf-8")
        except UnicodeError:
            fail("MALFORMED")
        self.limits, self.pos, self.count = limits, 0, 0

    def ws(self):
        while self.pos < len(self.text) and self.text[self.pos] in " \t\r\n":
            self.pos += 1

    def take(self):
        if self.pos == len(self.text):
            fail("MALFORMED")
        c = self.text[self.pos]
        self.pos += 1
        return c

    def eat(self, c):
        self.ws()
        if self.text[self.pos:self.pos+1] == c:
            self.pos += 1
            return True
        return False

    def hex4(self):
        s = self.text[self.pos:self.pos+4]
        if len(s) != 4 or any(c not in "0123456789abcdefABCDEF" for c in s):
            fail("MALFORMED")
        self.pos += 4
        return int(s, 16)

    def string(self):
        start = self.pos
        if self.take() != '"':
            fail("MALFORMED")
        size = 0
        while True:
            c = self.take()
            if c == '"':
                # Token budget was checked before the standard decoder allocates.
                return json.loads(self.text[start:self.pos])
            if ord(c) < 32:
                fail("MALFORMED")
            add = len(c.encode("utf-8"))
            if c == "\\":
                esc = self.take()
                if esc == "u":
                    cp = self.hex4()
                    if 0xD800 <= cp <= 0xDBFF:
                        if self.take() != "\\" or self.take() != "u":
                            fail("UNICODE")
                        low = self.hex4()
                        if not 0xDC00 <= low <= 0xDFFF:
                            fail("UNICODE")
                        cp = 0x10000 + ((cp - 0xD800) << 10) + low - 0xDC00
                    elif 0xDC00 <= cp <= 0xDFFF:
                        fail("UNICODE")
                    add = len(chr(cp).encode("utf-8"))
                elif esc not in '"\\/bfnrt':
                    fail("MALFORMED")
                else:
                    add = 1
            size += add
            if size > self.limits.max_string_length:
                fail("LIMIT")

    def value(self, depth=1):
        self.ws()
        if self.pos == len(self.text):
            fail("MALFORMED")
        if depth > self.limits.max_depth or self.count >= self.limits.max_items:
            fail("LIMIT")
        self.count += 1
        c = self.text[self.pos]
        if c in "{[":
            self.pos += 1
            obj = c == "{"
            result = {} if obj else []
            end = "}" if obj else "]"
            if self.eat(end):
                return result
            while True:
                self.ws()
                if obj:
                    key = self.string()
                    if key in result:
                        fail("DUPLICATE_KEY")
                    if not self.eat(":"):
                        fail("MALFORMED")
                    result[key] = self.value(depth+1)
                else:
                    result.append(self.value(depth+1))
                if self.eat(end):
                    return result
                if not self.eat(","):
                    fail("MALFORMED")
        if c == '"':
            return self.string()
        for literal, value in (("true", True), ("false", False), ("null", None)):
            if self.text.startswith(literal, self.pos):
                self.pos += len(literal)
                return value
        if any(self.text.startswith(t, self.pos) for t in ("NaN", "Infinity", "-Infinity")):
            fail("NUMBER")
        match = NUMBER.match(self.text, self.pos)
        if not match:
            fail("MALFORMED")
        token = match.group()
        self.pos = match.end()
        value = float(token)
        numeric(value)
        # A nonzero decimal that underflows binary64 is unsupported.
        if value == 0 and any(c in "123456789" for c in token.lower().split("e")[0]):
            fail("NUMBER")
        return value

    def run(self):
        value = self.value()
        self.ws()
        if self.pos != len(self.text):
            fail("MALFORMED")
        return value

def check_value(root, limits):
    limits.check()
    count = 0
    def string(s):
        try:
            size = len(s.encode("utf-8"))
        except UnicodeError:
            fail("UNICODE")
        if size > limits.max_string_length:
            fail("LIMIT")
    def walk(value, depth):
        nonlocal count
        if depth > limits.max_depth or count >= limits.max_items:
            fail("LIMIT")
        count += 1
        if isinstance(value, str):
            string(value)
        elif type(value) in (int, float):
            numeric(value)
        elif isinstance(value, dict):
            for k, v in value.items():
                if not isinstance(k, str):
                    fail("SCHEMA")
                string(k)
                walk(v, depth+1)
        elif isinstance(value, list):
            for v in value:
                walk(v, depth+1)
        elif value is not None and type(value) is not bool:
            fail("SCHEMA")
    walk(root, 1)

class JsonCodec:
    def decode(self, raw: bytes, limits=Limits()):
        return Parser(raw, limits).run()

    def encode(self, value, limits=Limits()):
        check_value(value, limits)
        result = bytearray()
        encoder = json.JSONEncoder(ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        for chunk in encoder.iterencode(value):
            encoded = chunk.encode("utf-8")
            if len(encoded) > limits.max_bytes - len(result):
                fail("LIMIT")
            result.extend(encoded)
        return bytes(result)

TYPES = {"vinox.provenance.snapshot": "vinox", "liara.heartbeat.snapshot": "heartbeat",
         "linep.v02.request": "linep"}
SCHEMA_DIR = Path(__file__).resolve().parents[1] / "schemas/pilot-v0.1"
SCHEMAS = {t: Draft202012Validator(json.loads((SCHEMA_DIR / f"{n}.schema.json").read_text(encoding="utf-8")))
           for t, n in TYPES.items()}
UNITS = dict(utilization_ratio="ratio", memory_used_ratio="ratio", temperature_c="celsius",
             power_w="watts", charge_ratio="ratio", charge_rate_w="watts",
             external_power_connected="boolean", queue_depth="count", active_work="count", available="boolean")

def uint64(text, nonzero=False):
    if not isinstance(text, str) or not UINT.fullmatch(text):
        fail("SCHEMA")
    n = int(text)
    if n < int(nonzero) or n > 18446744073709551615:
        fail("UINT64_RANGE")
    return n

def float32(n):
    try:
        restored = struct.unpack("!f", struct.pack("!f", n))[0]
    except (OverflowError, struct.error):
        fail("FLOAT32")
    if restored != n:
        fail("FLOAT32")

def timestamp(text):
    if not TIME.fullmatch(text):
        fail("SCHEMA")
    try:
        datetime.fromisoformat(text.replace("Z", "+00:00"))
    except ValueError:
        fail("TIMESTAMP")

def validate_pilot(root, expected_type, supported_versions):
    if not isinstance(root, dict):
        fail("SCHEMA")
    t = root.get("type_id")
    if not isinstance(t, str) or t not in TYPES or not expected_type or t != expected_type:
        fail("TYPE")
    if root.get("schema_version") not in supported_versions:
        fail("VERSION")
    if next(SCHEMAS[t].iter_errors(root), None) is not None:
        fail("SCHEMA")
    p, provenance = root["payload"], root["provenance"]
    if "timestamp_ms" in provenance:
        uint64(provenance["timestamp_ms"])
    if t == "liara.heartbeat.snapshot":
        uint64(p["sequence"])
        timestamp(p["observed_at"])
        for observation in p["observations"]:
            timestamp(observation["observed_at"])
            unit, value = observation["unit"], observation["value"]
            if UNITS[observation["metric"]] != unit:
                fail("METRIC_UNIT")
            if (unit == "ratio" and not 0 <= value <= 1 or
                unit == "count" and value < 0 or unit == "boolean" and value not in (0, 1)):
                fail("METRIC_VALUE")
    elif t == "linep.v02.request":
        uint64(p["stream"]["request_id"], True)
        uint64(p["stream"]["execution_id"], True)
        float32(p["temperature"])
        if p["has_options"]:
            o = p["options"]
            uint64(o["seed"])
            for field in ("top_p", "repeat_penalty", "presence_penalty", "frequency_penalty"):
                float32(o[field])
            keys = [pair[0].encode("utf-8") for pair in o["extra_options"]]
            if keys != sorted(keys) or len(keys) != len(set(keys)):
                fail("OPTION_KEYS")

def freeze(v):
    if isinstance(v, dict):
        return MappingProxyType({k: freeze(x) for k, x in v.items()})
    if isinstance(v, list):
        return tuple(freeze(x) for x in v)
    return v

def thaw(v):
    if isinstance(v, MappingProxyType):
        return {k: thaw(x) for k, x in v.items()}
    if isinstance(v, tuple):
        return [thaw(x) for x in v]
    return v

@dataclass(frozen=True, init=False)
class Envelope:
    _root: object

    def __init__(self, root, expected_type, supported_versions=("0.1",), limits=Limits()):
        check_value(root, limits)
        validate_pilot(root, expected_type, supported_versions)
        object.__setattr__(self, "_root", freeze(root))

    @classmethod
    def decode(cls, raw, expected_type, supported_versions=("0.1",), limits=Limits(), codec=None):
        codec = codec or JsonCodec()
        return cls(codec.decode(raw, limits), expected_type, supported_versions, limits)

    @property
    def value(self):
        return thaw(self._root)

    def encode(self, limits=Limits(), codec=None):
        return (codec or JsonCodec()).encode(self.value, limits)
