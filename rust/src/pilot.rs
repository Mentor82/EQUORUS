use std::collections::BTreeMap;
use crate::envelope::Envelope;
use crate::error::{fail, ErrorCode, Result};
use crate::limits::Limits;
use crate::value::{parse_uint64, unicode_length, Value};

const SUPPORTED_VERSIONS: [&str; 1] = ["0.1"];

fn as_object(v: &Value) -> Result<&BTreeMap<String, Value>> {
    match v {
        Value::Object(m) => Ok(m),
        _ => fail(ErrorCode::Schema),
    }
}

fn as_array(v: &Value) -> Result<&[Value]> {
    match v {
        Value::Array(a) => Ok(a),
        _ => fail(ErrorCode::Schema),
    }
}

fn as_string(v: &Value) -> Result<&str> {
    match v {
        Value::String(s) => Ok(s.as_str()),
        _ => fail(ErrorCode::Schema),
    }
}

fn as_number(v: &Value) -> Result<f64> {
    match v {
        Value::Number(n) => Ok(*n),
        _ => fail(ErrorCode::Schema),
    }
}

fn as_bool(v: &Value) -> Result<bool> {
    match v {
        Value::Bool(b) => Ok(*b),
        _ => fail(ErrorCode::Schema),
    }
}

fn check_keys(o: &BTreeMap<String, Value>, required: &[&str], optional: &[&str]) -> Result<()> {
    for r in required {
        if !o.contains_key(*r) {
            return fail(ErrorCode::Schema);
        }
    }
    for k in o.keys() {
        let is_req = required.contains(&k.as_str());
        let is_opt = optional.contains(&k.as_str());
        if !is_req && !is_opt {
            return fail(ErrorCode::Schema);
        }
    }
    Ok(())
}

fn check_one_of(v: &Value, choices: &[&str]) -> Result<()> {
    let s = as_string(v)?;
    if choices.contains(&s) {
        Ok(())
    } else {
        fail(ErrorCode::Schema)
    }
}

fn check_equal(v: &Value, expected: &str) -> Result<()> {
    if as_string(v)? == expected {
        Ok(())
    } else {
        fail(ErrorCode::Schema)
    }
}

fn check_integer(v: &Value, low: f64, high: f64) -> Result<()> {
    let n = as_number(v)?;
    if n.trunc() != n || n < low || n > high {
        fail(ErrorCode::Schema)
    } else {
        Ok(())
    }
}

fn check_ratio(v: &Value) -> Result<()> {
    let n = as_number(v)?;
    if (0.0..=1.0).contains(&n) {
        Ok(())
    } else {
        fail(ErrorCode::Schema)
    }
}

fn check_bounded_id(v: &Value) -> Result<()> {
    let s = as_string(v)?;
    let len = unicode_length(s)?;
    if len == 0 || len > 128 {
        fail(ErrorCode::Schema)
    } else {
        Ok(())
    }
}

fn check_f32(v: &Value) -> Result<()> {
    let n = as_number(v)?;
    if n.abs() > f32::MAX as f64 || (n as f32) as f64 != n {
        fail(ErrorCode::Float32)
    } else {
        Ok(())
    }
}

fn check_u64(v: &Value, nonzero: bool) -> Result<()> {
    parse_uint64(as_string(v)?, nonzero)?;
    Ok(())
}

fn check_timestamp(v: &Value) -> Result<()> {
    let s = as_string(v)?;
    let b = s.as_bytes();
    if b.len() < 20 {
        return fail(ErrorCode::Schema);
    }
    let is_digit = |c: u8| c.is_ascii_digit();
    if !is_digit(b[0]) || !is_digit(b[1]) || !is_digit(b[2]) || !is_digit(b[3])
        || b[4] != b'-' || !is_digit(b[5]) || !is_digit(b[6])
        || b[7] != b'-' || !is_digit(b[8]) || !is_digit(b[9])
        || b[10] != b'T' || !is_digit(b[11]) || !is_digit(b[12])
        || b[13] != b':' || !is_digit(b[14]) || !is_digit(b[15])
        || b[16] != b':' || !is_digit(b[17]) || !is_digit(b[18])
    {
        return fail(ErrorCode::Schema);
    }
    let mut pos = 19;
    if pos < b.len() && b[pos] == b'.' {
        pos += 1;
        let frac_start = pos;
        while pos < b.len() && is_digit(b[pos]) {
            pos += 1;
        }
        let frac_len = pos - frac_start;
        if frac_len < 1 || frac_len > 6 {
            return fail(ErrorCode::Schema);
        }
    }
    if pos >= b.len() {
        return fail(ErrorCode::Schema);
    }
    let tz = &s[pos..];
    if tz == "Z" {
        // ok
    } else if (tz.starts_with('+') || tz.starts_with('-')) && tz.len() == 6 && tz.as_bytes()[3] == b':' {
        let h_str = &tz[1..3];
        let m_str = &tz[4..6];
        let h: u32 = h_str.parse().map_err(|_| crate::error::Error { code: ErrorCode::Timestamp })?;
        let m: u32 = m_str.parse().map_err(|_| crate::error::Error { code: ErrorCode::Timestamp })?;
        if h > 23 || m > 59 {
            return fail(ErrorCode::Timestamp);
        }
    } else {
        return fail(ErrorCode::Schema);
    }

    let year: u32 = s[0..4].parse().map_err(|_| crate::error::Error { code: ErrorCode::Timestamp })?;
    let month: u32 = s[5..7].parse().map_err(|_| crate::error::Error { code: ErrorCode::Timestamp })?;
    let day: u32 = s[8..10].parse().map_err(|_| crate::error::Error { code: ErrorCode::Timestamp })?;
    let hour: u32 = s[11..13].parse().map_err(|_| crate::error::Error { code: ErrorCode::Timestamp })?;
    let min: u32 = s[14..16].parse().map_err(|_| crate::error::Error { code: ErrorCode::Timestamp })?;
    let sec: u32 = s[17..19].parse().map_err(|_| crate::error::Error { code: ErrorCode::Timestamp })?;

    if year < 1 || month < 1 || month > 12 {
        return fail(ErrorCode::Timestamp);
    }
    let leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    let days_in_month = match month {
        1 | 3 | 5 | 7 | 8 | 10 | 12 => 31,
        4 | 6 | 9 | 11 => 30,
        2 => if leap { 29 } else { 28 },
        _ => return fail(ErrorCode::Timestamp),
    };
    if day < 1 || day > days_in_month || hour > 23 || min > 59 || sec > 59 {
        return fail(ErrorCode::Timestamp);
    }
    Ok(())
}

fn validate_provenance(v: &Value) -> Result<()> {
    let o = as_object(v)?;
    check_keys(o, &["kind"], &["source_id", "timestamp_ms"])?;
    check_one_of(&o["kind"], &["SOURCE_LITERAL", "TOOL_EVIDENCE", "MODEL_GENERATED", "DERIVED_CONTEXT"])?;
    if let Some(sid) = o.get("source_id") {
        if sid != &Value::Null {
            as_string(sid)?;
        }
    }
    if let Some(ts) = o.get("timestamp_ms") {
        check_u64(ts, false)?;
    }
    Ok(())
}

fn metric_unit(metric: &str) -> Option<&'static str> {
    match metric {
        "utilization_ratio" | "memory_used_ratio" | "charge_ratio" => Some("ratio"),
        "temperature_c" => Some("celsius"),
        "power_w" | "charge_rate_w" => Some("watts"),
        "external_power_connected" | "available" => Some("boolean"),
        "queue_depth" | "active_work" => Some("count"),
        _ => None,
    }
}

fn validate_observation(v: &Value) -> Result<()> {
    let o = as_object(v)?;
    check_keys(
        o,
        &["resource", "metric", "value", "unit", "device_id", "observed_at", "source_id", "confidence", "attributes"],
        &[],
    )?;
    check_one_of(&o["resource"], &["cpu", "ram", "gpu", "npu", "battery", "thermal", "power", "system"])?;
    check_one_of(&o["unit"], &["ratio", "celsius", "watts", "count", "boolean"])?;
    let metric_str = as_string(&o["metric"])?;
    let expected_unit = metric_unit(metric_str).ok_or(crate::error::Error { code: ErrorCode::Schema })?;
    let unit_str = as_string(&o["unit"])?;
    check_bounded_id(&o["device_id"])?;
    check_bounded_id(&o["source_id"])?;
    check_timestamp(&o["observed_at"])?;
    check_ratio(&o["confidence"])?;
    if !as_object(&o["attributes"])?.is_empty() {
        return fail(ErrorCode::Schema);
    }
    let n = as_number(&o["value"])?;
    if expected_unit != unit_str {
        return fail(ErrorCode::MetricUnit);
    }
    if (unit_str == "ratio" && !(0.0..=1.0).contains(&n))
        || (unit_str == "count" && n < 0.0)
        || (unit_str == "boolean" && n != 0.0 && n != 1.0)
    {
        return fail(ErrorCode::MetricValue);
    }
    Ok(())
}

fn validate_heartbeat(v: &Value) -> Result<()> {
    let o = as_object(v)?;
    check_keys(
        o,
        &["schema_version", "instance_id", "instance_type", "node_id", "sequence", "observed_at", "state", "observations", "signals", "confidence"],
        &[],
    )?;
    check_equal(&o["schema_version"], "1.0")?;
    check_equal(&o["instance_type"], "heartbeat")?;
    as_string(&o["instance_id"])?;
    as_string(&o["node_id"])?;
    check_u64(&o["sequence"], false)?;
    check_one_of(&o["state"], &["healthy", "constrained", "degraded", "critical", "unknown"])?;
    check_timestamp(&o["observed_at"])?;
    check_ratio(&o["confidence"])?;
    for s in as_array(&o["signals"])? {
        as_string(s)?;
    }
    for obs in as_array(&o["observations"])? {
        validate_observation(obs)?;
    }
    Ok(())
}

fn validate_options(v: &Value) -> Result<()> {
    let o = as_object(v)?;
    check_keys(
        o,
        &["top_p", "top_k", "repeat_penalty", "repeat_last_n", "seed", "presence_penalty", "frequency_penalty", "stop_sequences", "extra_options"],
        &[],
    )?;
    for name in &["top_p", "repeat_penalty", "presence_penalty", "frequency_penalty"] {
        check_f32(&o[*name])?;
    }
    for name in &["top_k", "repeat_last_n"] {
        check_integer(&o[*name], -2147483648.0, 2147483647.0)?;
    }
    check_u64(&o["seed"], false)?;
    for s in as_array(&o["stop_sequences"])? {
        as_string(s)?;
    }
    let mut last: Option<&str> = None;
    for x in as_array(&o["extra_options"])? {
        let pair = as_array(x)?;
        if pair.len() != 2 {
            return fail(ErrorCode::Schema);
        }
        let key = as_string(&pair[0])?;
        as_string(&pair[1])?;
        if let Some(prev) = last {
            if prev.as_bytes() >= key.as_bytes() {
                return fail(ErrorCode::OptionKeys);
            }
        }
        last = Some(key);
    }
    Ok(())
}

fn validate_request(v: &Value) -> Result<()> {
    let o = as_object(v)?;
    check_keys(
        o,
        &["protocol_version", "stream", "profile", "model_id", "payload", "max_tokens", "temperature", "stream_requested", "has_options"],
        &["options"],
    )?;
    check_equal(&o["protocol_version"], "0.2")?;
    check_one_of(&o["profile"], &["generate", "chat", "embed"])?;
    if as_string(&o["model_id"])?.is_empty() {
        return fail(ErrorCode::Schema);
    }
    as_string(&o["payload"])?;
    as_bool(&o["stream_requested"])?;
    check_integer(&o["max_tokens"], 0.0, 4294967295.0)?;
    let stream = as_object(&o["stream"])?;
    check_keys(stream, &["request_id", "execution_id", "output_id"], &[])?;
    check_u64(&stream["request_id"], true)?;
    check_u64(&stream["execution_id"], true)?;
    check_integer(&stream["output_id"], 0.0, 4294967295.0)?;
    check_f32(&o["temperature"])?;
    let has = as_bool(&o["has_options"])?;
    let has_field = o.contains_key("options");
    if has != has_field {
        return fail(ErrorCode::Schema);
    }
    if has {
        validate_options(&o["options"])?;
    }
    Ok(())
}

pub fn validate_pilot(root: &Value) -> Result<()> {
    let o = as_object(root)?;
    check_keys(o, &["type_id", "schema_version", "provenance", "payload"], &[])?;
    let type_id = as_string(&o["type_id"])?;
    if type_id != "vinox.provenance.snapshot" && type_id != "liara.heartbeat.snapshot" && type_id != "linep.v02.request" {
        return fail(ErrorCode::Type);
    }
    check_equal(&o["schema_version"], "0.1")?;
    validate_provenance(&o["provenance"])?;
    if type_id == "vinox.provenance.snapshot" {
        if !as_object(&o["payload"])?.is_empty() {
            return fail(ErrorCode::Schema);
        }
    } else if type_id == "liara.heartbeat.snapshot" {
        validate_heartbeat(&o["payload"])?;
    } else {
        validate_request(&o["payload"])?;
    }
    Ok(())
}

pub fn decode_pilot(raw: &[u8], expected: &str, limits: &Limits) -> Result<Envelope> {
    Envelope::decode(raw, expected, &SUPPORTED_VERSIONS, validate_pilot, limits)
}

pub fn create_pilot(val: Value, expected: &str, limits: &Limits) -> Result<Envelope> {
    Envelope::create(val, expected, &SUPPORTED_VERSIONS, validate_pilot, limits)
}
