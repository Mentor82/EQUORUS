"""Test suite for the LIARA Heartbeat opt-in adapter (M4)."""
from datetime import datetime, timezone, timedelta
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))
sys.path.insert(0, "c:/ai/LIARA")

from equorus_reference import Envelope, Limits, ContractError
try:
    from equorus_liara import heartbeat_to_envelope, envelope_to_heartbeat
    from services.contracts.heartbeat import HeartbeatSnapshot, ResourceObservation
except ImportError as _err:
    print(f"SKIP: LIARA heartbeat contracts not available on this host ({_err})")
    sys.exit(0)

FIXTURES = ROOT / "tests/fixtures/pilot-v0.1"


def test_fixture_heartbeat_roundtrip():
    raw = (FIXTURES / "heartbeat.json").read_bytes()
    golden_env = Envelope.decode(raw, "liara.heartbeat.snapshot")

    snapshot, prov = envelope_to_heartbeat(golden_env)
    assert snapshot.instance_id == "fixture-heartbeat"
    assert snapshot.node_id == "fixture-node"
    assert snapshot.sequence == 9007199254740993
    assert snapshot.state == "healthy"
    assert snapshot.confidence == 0.75
    assert len(snapshot.observations) == 1
    assert snapshot.observations[0].metric == "utilization_ratio"
    assert snapshot.observations[0].unit == "ratio"
    assert snapshot.observations[0].value == 0.5
    assert snapshot.observations[0].attributes == {}
    assert snapshot.observed_at.microsecond == 123456

    re_env = heartbeat_to_envelope(snapshot, provenance=prov)
    assert re_env.value == golden_env.value

    # Re-encoded JSON roundtrip
    re_json = json.loads(re_env.encode())
    golden_json = json.loads(raw)
    assert re_json == golden_json


def test_uint64_sequence_bounds():
    now = datetime(2026, 10, 1, 12, 0, 0, 654321, tzinfo=timezone.utc)
    obs = ResourceObservation(
        resource="ram",
        metric="memory_used_ratio",
        value=0.42,
        unit="ratio",
        device_id="default",
        observed_at=now,
        source_id="fixture:sensor",
        confidence=1.0,
        attributes={},
    )

    # Upper bound uint64: 2^64 - 1
    snap_max = HeartbeatSnapshot(
        schema_version="1.0",
        instance_id="hb-max",
        instance_type="heartbeat",
        node_id="node-1",
        sequence=18446744073709551615,
        observed_at=now,
        state="healthy",
        observations=[obs],
        signals=["ok"],
        confidence=1.0,
    )
    env = heartbeat_to_envelope(snap_max)
    assert env.value["payload"]["sequence"] == "18446744073709551615"
    back, _ = envelope_to_heartbeat(env)
    assert back.sequence == 18446744073709551615

    # Sequence > uint64 max rejected
    snap_overflow = HeartbeatSnapshot(
        schema_version="1.0",
        instance_id="hb-overflow",
        instance_type="heartbeat",
        node_id="node-1",
        sequence=18446744073709551616,
        observed_at=now,
        state="healthy",
        observations=[obs],
        signals=[],
        confidence=1.0,
    )
    try:
        heartbeat_to_envelope(snap_overflow)
        assert False, "expected ContractError(UINT64_RANGE)"
    except ContractError as e:
        assert str(e) == "UINT64_RANGE"


def test_empty_attributes_strictly_enforced():
    now = datetime(2026, 10, 1, 12, 0, 0, tzinfo=timezone.utc)
    obs = ResourceObservation(
        resource="ram",
        metric="memory_used_ratio",
        value=0.5,
        unit="ratio",
        device_id="default",
        observed_at=now,
        source_id="fixture:sensor",
        confidence=1.0,
        attributes={"core": 1},  # Non-empty attributes
    )
    snap = HeartbeatSnapshot(
        schema_version="1.0",
        instance_id="hb-attrs",
        instance_type="heartbeat",
        node_id="node-1",
        sequence=1,
        observed_at=now,
        state="healthy",
        observations=[obs],
        signals=[],
        confidence=1.0,
    )
    try:
        heartbeat_to_envelope(snap)
        assert False, "expected ContractError(SCHEMA) for non-empty attributes"
    except ContractError as e:
        assert str(e) == "SCHEMA"


def test_multi_resource_observations_and_timezone():
    tz_custom = timezone(timedelta(hours=5, minutes=30))
    t1 = datetime(2026, 10, 1, 8, 30, 45, 999999, tzinfo=tz_custom)
    t2 = datetime(2026, 10, 1, 8, 30, 46, 1, tzinfo=tz_custom)

    observations = [
        ResourceObservation(
            resource="cpu",
            metric="temperature_c",
            value=65.5,
            unit="celsius",
            device_id="cpu0",
            observed_at=t1,
            source_id="sensor:thermal",
            confidence=0.95,
            attributes={},
        ),
        ResourceObservation(
            resource="power",
            metric="power_w",
            value=45.0,
            unit="watts",
            device_id="psu0",
            observed_at=t2,
            source_id="sensor:power",
            confidence=1.0,
            attributes={},
        ),
        ResourceObservation(
            resource="system",
            metric="available",
            value=1.0,
            unit="boolean",
            device_id="host",
            observed_at=t2,
            source_id="sensor:sys",
            confidence=1.0,
            attributes={},
        ),
    ]

    snap = HeartbeatSnapshot(
        schema_version="1.0",
        instance_id="multi-obs",
        instance_type="heartbeat",
        node_id="node-omega",
        sequence=42,
        observed_at=t2,
        state="constrained",
        observations=observations,
        signals=["thermal_warning"],
        confidence=0.88,
    )

    env = heartbeat_to_envelope(snap)
    back, _ = envelope_to_heartbeat(env)

    assert len(back.observations) == 3
    assert back.observations[0].metric == "temperature_c"
    assert back.observations[0].value == 65.5
    assert back.observations[0].observed_at == t1
    assert back.observations[0].observed_at.microsecond == 999999
    assert back.observations[1].metric == "power_w"
    assert back.observations[1].value == 45.0
    assert back.observations[2].metric == "available"
    assert back.observations[2].value == 1.0
    assert back.state == "constrained"
    assert back.signals == ["thermal_warning"]


def main():
    test_fixture_heartbeat_roundtrip()
    test_uint64_sequence_bounds()
    test_empty_attributes_strictly_enforced()
    test_multi_resource_observations_and_timezone()
    print("All LIARA heartbeat adapter tests passed successfully.")


if __name__ == "__main__":
    main()
