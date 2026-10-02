"""LIARA Heartbeat opt-in adapter for EQUORUS envelopes (pilot v0.1).

Converts bidirectionally between LIARA HeartbeatSnapshot contracts and
EQUORUS liara.heartbeat.snapshot envelopes without loss of precision.
"""
from __future__ import annotations

from datetime import datetime
from pathlib import Path
import sys
from typing import Any

# Ensure LIARA contracts can be resolved
LIARA_ROOT = Path("c:/ai/LIARA")
if LIARA_ROOT.exists() and str(LIARA_ROOT) not in sys.path:
    sys.path.insert(0, str(LIARA_ROOT))

try:
    from services.contracts.heartbeat import HeartbeatSnapshot, ResourceObservation
except ImportError:
    # Fallback to local import if already in path or mocked
    try:
        from contracts.heartbeat import HeartbeatSnapshot, ResourceObservation  # type: ignore
    except ImportError as e:
        raise ImportError(f"Could not import LIARA heartbeat contracts: {e}")

try:
    from equorus_reference import Envelope, Limits, ContractError
except ImportError:
    from .equorus_reference import Envelope, Limits, ContractError

UINT64_MAX = 18446744073709551615

EXPECTED_UNITS = {
    "utilization_ratio": "ratio",
    "memory_used_ratio": "ratio",
    "temperature_c": "celsius",
    "power_w": "watts",
    "charge_ratio": "ratio",
    "charge_rate_w": "watts",
    "external_power_connected": "boolean",
    "queue_depth": "count",
    "active_work": "count",
    "available": "boolean",
}


def _format_datetime(dt: datetime) -> str:
    if dt.tzinfo is None:
        raise ContractError("TIMESTAMP")
    # Preserve microseconds and timezone offset exactly
    return dt.isoformat()


def heartbeat_to_envelope(
    snapshot: HeartbeatSnapshot,
    provenance: dict[str, Any] | None = None,
    limits: Limits = Limits(),
) -> Envelope:
    """Convert a native LIARA HeartbeatSnapshot to an EQUORUS envelope.

    Enforces contract invariants:
    - sequence in [0, 2^64-1] as decimal string
    - observation attributes must be empty in pilot v0.1
    - timestamps retain timezone and microsecond resolution
    """
    if snapshot.sequence < 0 or snapshot.sequence > UINT64_MAX:
        raise ContractError("UINT64_RANGE")

    if provenance is None:
        provenance = {
            "kind": "SOURCE_LITERAL",
            "source_id": f"liara:{snapshot.instance_id}",
        }

    observations_payload = []
    for obs in snapshot.observations:
        if obs.attributes:
            # Silent dropping or partial attribute loss is strictly forbidden
            raise ContractError("SCHEMA")
        if obs.metric not in EXPECTED_UNITS or obs.unit != EXPECTED_UNITS[obs.metric]:
            raise ContractError("METRIC_UNIT")

        obs_dict = {
            "resource": obs.resource,
            "metric": obs.metric,
            "value": obs.value,
            "unit": obs.unit,
            "device_id": obs.device_id,
            "observed_at": _format_datetime(obs.observed_at),
            "source_id": obs.source_id,
            "confidence": obs.confidence,
            "attributes": {},
        }
        observations_payload.append(obs_dict)

    payload = {
        "schema_version": "1.0",
        "instance_id": snapshot.instance_id,
        "instance_type": "heartbeat",
        "node_id": snapshot.node_id,
        "sequence": str(snapshot.sequence),
        "observed_at": _format_datetime(snapshot.observed_at),
        "state": snapshot.state,
        "observations": observations_payload,
        "signals": list(snapshot.signals),
        "confidence": snapshot.confidence,
    }

    root = {
        "type_id": "liara.heartbeat.snapshot",
        "schema_version": "0.1",
        "provenance": provenance,
        "payload": payload,
    }

    return Envelope(root, "liara.heartbeat.snapshot", limits=limits)


def envelope_to_heartbeat(
    env: Envelope,
) -> tuple[HeartbeatSnapshot, dict[str, Any]]:
    """Convert an EQUORUS envelope to a native LIARA HeartbeatSnapshot and provenance."""
    val = env.value
    if val.get("type_id") != "liara.heartbeat.snapshot":
        raise ContractError("TYPE")

    payload = val["payload"]
    provenance = val.get("provenance", {})

    observations = []
    for obs_dict in payload["observations"]:
        if obs_dict.get("attributes"):
            raise ContractError("SCHEMA")

        obs = ResourceObservation(
            resource=obs_dict["resource"],
            metric=obs_dict["metric"],
            value=obs_dict["value"],
            unit=obs_dict["unit"],
            device_id=obs_dict["device_id"],
            observed_at=datetime.fromisoformat(obs_dict["observed_at"]),
            source_id=obs_dict["source_id"],
            confidence=obs_dict["confidence"],
            attributes={},
        )
        observations.append(obs)

    snapshot = HeartbeatSnapshot(
        schema_version="1.0",
        instance_id=payload["instance_id"],
        instance_type="heartbeat",
        node_id=payload["node_id"],
        sequence=int(payload["sequence"]),
        observed_at=datetime.fromisoformat(payload["observed_at"]),
        state=payload["state"],
        observations=observations,
        signals=list(payload.get("signals", [])),
        confidence=payload["confidence"],
    )

    return snapshot, provenance
