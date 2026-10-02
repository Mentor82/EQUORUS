use std::collections::BTreeMap;
use crate::error::{fail, ErrorCode, Result};
use crate::limits::Limits;

#[derive(Debug, Clone, PartialEq)]
pub enum Value {
    Null,
    Bool(bool),
    Number(f64),
    String(String),
    Array(Vec<Value>),
    Object(BTreeMap<String, Value>),
}

pub fn validate_number(v: f64) -> Result<()> {
    if !v.is_finite() || (v == 0.0 && v.is_sign_negative()) {
        return fail(ErrorCode::Number);
    }
    if v.trunc() == v && v.abs() > 9007199254740991.0 {
        return fail(ErrorCode::Number);
    }
    Ok(())
}

pub fn unicode_length(s: &str) -> Result<usize> {
    let mut count = 0;
    let bytes = s.as_bytes();
    let mut i = 0;
    let n_bytes = bytes.len();
    while i < n_bytes {
        let c = bytes[i];
        i += 1;
        count += 1;
        if c < 0x80 {
            continue;
        }
        let n: usize;
        let mut cp: u32;
        let minimum: u32;
        if (0xC2..=0xDF).contains(&c) {
            n = 1;
            cp = (c & 31) as u32;
            minimum = 0x80;
        } else if (0xE0..=0xEF).contains(&c) {
            n = 2;
            cp = (c & 15) as u32;
            minimum = 0x800;
        } else if (0xF0..=0xF4).contains(&c) {
            n = 3;
            cp = (c & 7) as u32;
            minimum = 0x10000;
        } else {
            return fail(ErrorCode::Unicode);
        }

        if n > n_bytes - i {
            return fail(ErrorCode::Unicode);
        }
        for _ in 0..n {
            let b = bytes[i];
            i += 1;
            if (b & 0xC0) != 0x80 {
                return fail(ErrorCode::Unicode);
            }
            cp = (cp << 6) | ((b & 63) as u32);
        }
        if cp < minimum || cp > 0x10FFFF || (0xD800..=0xDFFF).contains(&cp) {
            return fail(ErrorCode::Unicode);
        }
    }
    Ok(count)
}

pub fn parse_uint64(s: &str, nonzero: bool) -> Result<u64> {
    if s.is_empty() || (s.len() > 1 && s.starts_with('0')) || s.starts_with('-') || s.starts_with('+') {
        return fail(ErrorCode::Schema);
    }
    for b in s.bytes() {
        if !b.is_ascii_digit() {
            return fail(ErrorCode::Schema);
        }
    }
    let val: u64 = s.parse().map_err(|_| crate::error::Error {
        code: ErrorCode::Uint64Range,
    })?;
    if nonzero && val == 0 {
        return fail(ErrorCode::Uint64Range);
    }
    Ok(val)
}

pub fn validate_value(v: &Value, limits: &Limits) -> Result<()> {
    limits.check()?;
    let mut count = 0;
    fn walk(v: &Value, limits: &Limits, depth: usize, count: &mut usize) -> Result<()> {
        if depth > limits.max_depth || *count >= limits.max_items {
            return fail(ErrorCode::Limit);
        }
        *count += 1;
        match v {
            Value::Null | Value::Bool(_) => Ok(()),
            Value::Number(n) => validate_number(*n),
            Value::String(s) => {
                if s.len() > limits.max_string_length {
                    return fail(ErrorCode::Limit);
                }
                unicode_length(s)?;
                Ok(())
            }
            Value::Array(arr) => {
                for item in arr {
                    walk(item, limits, depth + 1, count)?;
                }
                Ok(())
            }
            Value::Object(obj) => {
                for (k, item) in obj {
                    if k.len() > limits.max_string_length {
                        return fail(ErrorCode::Limit);
                    }
                    unicode_length(k)?;
                    walk(item, limits, depth + 1, count)?;
                }
                Ok(())
            }
        }
    }
    walk(v, limits, 1, &mut count)
}
