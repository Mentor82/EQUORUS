"""Offline pilot-fixture checker, NOT a production/network JSON decoder.

No consumer code is imported or executed. Depth/item checks happen after parsing.
Requires jsonschema 4.x; all schemas resolve locally without network requests.
"""
from __future__ import annotations

import copy
from datetime import datetime
import json
import math
from pathlib import Path
import struct

from jsonschema import Draft202012Validator

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "tests/fixtures/pilot-v0.1"
SCHEMAS = ROOT / "schemas/pilot-v0.1"
DEFAULT_LIMITS = dict(max_bytes=65536, max_depth=12, max_items=2048, max_string_length=8192)
TYPES = {"vinox.provenance.snapshot": "vinox", "liara.heartbeat.snapshot": "heartbeat", "linep.v02.request": "linep"}
UNITS = dict(utilization_ratio="ratio", memory_used_ratio="ratio", temperature_c="celsius",
             power_w="watts", charge_ratio="ratio", charge_rate_w="watts",
             external_power_connected="boolean", queue_depth="count", active_work="count", available="boolean")


class ContractError(ValueError):
    def __init__(self, code: str):
        super().__init__(code)
        self.code = code


def reject(code: str):
    raise ContractError(code)


def pairs_unique(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            reject("DUPLICATE_KEY")
        result[key] = value
    return result


def parse_integer(token):
    if token == "-0":
        reject("NUMBER")
    return int(token)


def parse_float(token):
    value = float(token)
    if value == 0 and any(c in "123456789" for c in token.lower().split("e")[0]):
        reject("NUMBER")
    return value


def check_values(value, limits, depth=1, count=None):
    if count is None:
        count = [0]
    count[0] += 1
    if depth > limits["max_depth"] or count[0] > limits["max_items"]:
        reject("LIMIT")

    def check_string(s):
        try:
            size = len(s.encode("utf-8", errors="strict"))
        except UnicodeError:
            reject("UNICODE")
        if size > limits["max_string_length"]:
            reject("LIMIT")

    if isinstance(value, str):
        check_string(value)
    elif type(value) in (int, float):
        if isinstance(value, float) and (not math.isfinite(value) or
                (value == 0 and math.copysign(1, value) < 0)):
            reject("NUMBER")
        if (isinstance(value, int) or value.is_integer()) and abs(value) > 9007199254740991:
            reject("NUMBER")
    elif isinstance(value, dict):
        for key, child in value.items():
            check_string(key)
            check_values(child, limits, depth + 1, count)
    elif isinstance(value, list):
        for child in value:
            check_values(child, limits, depth + 1, count)


def u64(value, nonzero=False):
    # Shape/decimal grammar is checked by JSON Schema before semantic validation.
    if not (int(nonzero) <= int(value) <= 18446744073709551615):
        reject("UINT64_RANGE")


def f32(value):
    try:
        restored = struct.unpack("!f", struct.pack("!f", value))[0]
    except (OverflowError, struct.error):
        reject("FLOAT32")
    if restored != value:
        reject("FLOAT32")


def timestamp(value):
    try:
        result = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError:
        reject("TIMESTAMP")
    if result.tzinfo is None:
        reject("TIMESTAMP")


def validate(raw: bytes, validators, expected_type=None, limits=None):
    budget = DEFAULT_LIMITS | (limits or {})
    if len(raw) > budget["max_bytes"]:
        reject("LIMIT")
    try:
        value = json.loads(raw.decode("utf-8"), object_pairs_hook=pairs_unique,
                           parse_int=parse_integer, parse_float=parse_float,
                           parse_constant=lambda _: reject("NUMBER"))
    except (UnicodeError, json.JSONDecodeError, RecursionError):
        reject("MALFORMED")
    check_values(value, budget)
    if not isinstance(value, dict):
        reject("SCHEMA")
    type_id = value.get("type_id")
    if not isinstance(type_id, str) or type_id not in TYPES or (expected_type and type_id != expected_type):
        reject("TYPE")
    if value.get("schema_version") != "0.1":
        reject("VERSION")
    if next(validators[type_id].iter_errors(value), None) is not None:
        reject("SCHEMA")
    provenance = value["provenance"]
    if "timestamp_ms" in provenance:
        u64(provenance["timestamp_ms"])
    payload = value["payload"]
    if type_id == "liara.heartbeat.snapshot":
        u64(payload["sequence"])
        timestamp(payload["observed_at"])
        for observation in payload["observations"]:
            timestamp(observation["observed_at"])
            unit, number = observation["unit"], observation["value"]
            if UNITS[observation["metric"]] != unit:
                reject("METRIC_UNIT")
            if ((unit == "ratio" and not 0 <= number <= 1) or
                    (unit == "count" and number < 0) or
                    (unit == "boolean" and number not in (0, 1))):
                reject("METRIC_VALUE")
    elif type_id == "linep.v02.request":
        u64(payload["stream"]["request_id"], nonzero=True)
        u64(payload["stream"]["execution_id"], nonzero=True)
        f32(payload["temperature"])
        if payload["has_options"]:
            options = payload["options"]
            u64(options["seed"])
            for name in ("top_p", "repeat_penalty", "presence_penalty", "frequency_penalty"):
                f32(options[name])
            keys = [pair[0].encode("utf-8") for pair in options["extra_options"]]
            if keys != sorted(keys) or len(keys) != len(set(keys)):
                reject("OPTION_KEYS")
    return value


def apply_changes(value, changes):
    result = copy.deepcopy(value)
    for change in changes:
        parent = result
        for part in change["path"][:-1]:
            parent = parent[part]
        key = change["path"][-1]
        if change["op"] == "remove":
            del parent[key]
        elif change["op"] == "set":
            parent[key] = change["value"]
        else:
            raise ValueError(f"Unknown case operation: {change['op']}")
    return result


def main():
    validators = {}
    for type_id, name in TYPES.items():
        schema = json.loads((SCHEMAS / f"{name}.schema.json").read_text(encoding="utf-8"))
        Draft202012Validator.check_schema(schema)
        # Check timestamps below with stdlib, avoiding optional format extras
        # changing the rejection category between development environments.
        validators[type_id] = Draft202012Validator(schema)
    suite = json.loads((FIXTURES / "cases.json").read_text(encoding="utf-8"))
    covered = {case["base"] for case in suite if "base" in case}
    present = {p.name for p in FIXTURES.glob("*.json") if p.name != "cases.json"}
    if covered != present:
        raise AssertionError(f"Uncovered/missing fixtures: {covered ^ present}")
    passed = 0
    for case in suite:
        if "raw" in case:
            raw = case["raw"].encode("utf-8")
        elif "raw_hex" in case:
            raw = bytes.fromhex(case["raw_hex"])
        else:
            raw = (FIXTURES / case["base"]).read_bytes()
            if case.get("changes"):
                value = apply_changes(json.loads(raw), case["changes"])
                raw = json.dumps(value, ensure_ascii=True, allow_nan=False).encode("utf-8")
        actual = "accept"
        try:
            validate(raw, validators, case.get("expected_type"), case.get("limits"))
        except ContractError as error:
            actual = error.code
        if actual != case["expect"]:
            raise AssertionError(f"{case['name']}: expected {case['expect']}, got {actual}")
        passed += 1
    print(f"PASS: {len(validators)} draft schemas, {passed} contract fixture cases")
    print("Offline fixture validation only; no native roundtrip, wire or production decoder claim.")


if __name__ == "__main__":
    main()
