use crate::error::{fail, ErrorCode, Result};
use crate::limits::Limits;
use crate::value::{validate_number, validate_value, Value};

pub fn encode_json(v: &Value, limits: &Limits) -> Result<Vec<u8>> {
    validate_value(v, limits)?;
    let mut out = Vec::with_capacity(128);
    append_value(v, limits, &mut out)?;
    Ok(out)
}

fn append_bytes(bytes: &[u8], limits: &Limits, out: &mut Vec<u8>) -> Result<()> {
    if out.len() + bytes.len() > limits.max_bytes {
        return fail(ErrorCode::Limit);
    }
    out.extend_from_slice(bytes);
    Ok(())
}

fn append_string(s: &str, limits: &Limits, out: &mut Vec<u8>) -> Result<()> {
    append_bytes(b"\"", limits, out)?;
    for b in s.bytes() {
        match b {
            b'"' => append_bytes(b"\\\"", limits, out)?,
            b'\\' => append_bytes(b"\\\\", limits, out)?,
            c if c < 0x20 => {
                let esc = format!("\\u{:04x}", c);
                append_bytes(esc.as_bytes(), limits, out)?;
            }
            c => append_bytes(&[c], limits, out)?,
        }
    }
    append_bytes(b"\"", limits, out)?;
    Ok(())
}

fn append_value(v: &Value, limits: &Limits, out: &mut Vec<u8>) -> Result<()> {
    match v {
        Value::Null => append_bytes(b"null", limits, out),
        Value::Bool(true) => append_bytes(b"true", limits, out),
        Value::Bool(false) => append_bytes(b"false", limits, out),
        Value::Number(n) => {
            validate_number(*n)?;
            let s = if n.fract() == 0.0 && n.abs() <= 9007199254740991.0 {
                format!("{}", *n as i64)
            } else {
                format!("{}", n)
            };
            append_bytes(s.as_bytes(), limits, out)
        }
        Value::String(s) => append_string(s, limits, out),
        Value::Array(arr) => {
            append_bytes(b"[", limits, out)?;
            for (i, item) in arr.iter().enumerate() {
                if i > 0 {
                    append_bytes(b",", limits, out)?;
                }
                append_value(item, limits, out)?;
            }
            append_bytes(b"]", limits, out)
        }
        Value::Object(obj) => {
            append_bytes(b"{", limits, out)?;
            for (i, (k, val)) in obj.iter().enumerate() {
                if i > 0 {
                    append_bytes(b",", limits, out)?;
                }
                append_string(k, limits, out)?;
                append_bytes(b":", limits, out)?;
                append_value(val, limits, out)?;
            }
            append_bytes(b"}", limits, out)
        }
    }
}
