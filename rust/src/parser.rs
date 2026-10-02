use std::collections::BTreeMap;
use crate::error::{fail, Error, ErrorCode, Result};
use crate::limits::Limits;
use crate::value::{validate_number, Value};

struct Parser<'a> {
    data: &'a [u8],
    pos: usize,
    limits: Limits,
    items: usize,
}

pub fn parse_json(raw: &[u8], limits: &Limits) -> Result<Value> {
    limits.check()?;
    if raw.len() > limits.max_bytes {
        return fail(ErrorCode::Limit);
    }
    if std::str::from_utf8(raw).is_err() {
        return fail(ErrorCode::Malformed);
    }
    let mut p = Parser {
        data: raw,
        pos: 0,
        limits: *limits,
        items: 0,
    };
    let val = p.parse_value(1)?;
    p.skip_ws();
    if p.pos != p.data.len() {
        return fail(ErrorCode::Malformed);
    }
    Ok(val)
}

impl<'a> Parser<'a> {
    fn skip_ws(&mut self) {
        while self.pos < self.data.len() {
            let c = self.data[self.pos];
            if c == b' ' || c == b'\t' || c == b'\r' || c == b'\n' {
                self.pos += 1;
            } else {
                break;
            }
        }
    }

    fn take(&mut self) -> Result<u8> {
        if self.pos >= self.data.len() {
            return fail(ErrorCode::Malformed);
        }
        let c = self.data[self.pos];
        self.pos += 1;
        Ok(c)
    }

    fn hex4(&mut self) -> Result<u32> {
        if self.pos + 4 > self.data.len() {
            return fail(ErrorCode::Malformed);
        }
        let mut val = 0u32;
        for i in 0..4 {
            let c = self.data[self.pos + i];
            val <<= 4;
            if c.is_ascii_digit() {
                val |= (c - b'0') as u32;
            } else if (b'a'..=b'f').contains(&c) {
                val |= (c - b'a' + 10) as u32;
            } else if (b'A'..=b'F').contains(&c) {
                val |= (c - b'A' + 10) as u32;
            } else {
                return fail(ErrorCode::Malformed);
            }
        }
        self.pos += 4;
        Ok(val)
    }

    fn parse_string(&mut self) -> Result<String> {
        if self.take()? != b'"' {
            return fail(ErrorCode::Malformed);
        }
        let mut s = String::new();
        while self.pos < self.data.len() {
            let c = self.take()?;
            if c == b'"' {
                return Ok(s);
            }
            if c < 0x20 {
                return fail(ErrorCode::Malformed);
            }
            if c == b'\\' {
                let esc = self.take()?;
                match esc {
                    b'"' | b'\\' | b'/' => s.push(esc as char),
                    b'b' => s.push('\x08'),
                    b'f' => s.push('\x0c'),
                    b'n' => s.push('\n'),
                    b'r' => s.push('\r'),
                    b't' => s.push('\t'),
                    b'u' => {
                        let mut cp = self.hex4()?;
                        if (0xD800..=0xDBFF).contains(&cp) {
                            if self.pos + 2 > self.data.len() || self.take()? != b'\\' || self.take()? != b'u' {
                                return fail(ErrorCode::Unicode);
                            }
                            let low = self.hex4()?;
                            if !(0xDC00..=0xDFFF).contains(&low) {
                                return fail(ErrorCode::Unicode);
                            }
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                        } else if (0xDC00..=0xDFFF).contains(&cp) {
                            return fail(ErrorCode::Unicode);
                        }
                        let ch = char::from_u32(cp).ok_or(Error { code: ErrorCode::Unicode })?;
                        s.push(ch);
                    }
                    _ => return fail(ErrorCode::Malformed),
                }
            } else {
                self.pos -= 1;
                let remaining = &self.data[self.pos..];
                let s_ref = std::str::from_utf8(remaining).map_err(|_| Error { code: ErrorCode::Malformed })?;
                let ch = s_ref.chars().next().ok_or(Error { code: ErrorCode::Malformed })?;
                self.pos += ch.len_utf8();
                s.push(ch);
            }
            if s.len() > self.limits.max_string_length {
                return fail(ErrorCode::Limit);
            }
        }
        fail(ErrorCode::Malformed)
    }

    fn parse_number(&mut self) -> Result<f64> {
        let start = self.pos;
        if self.pos < self.data.len() && self.data[self.pos] == b'-' {
            self.pos += 1;
        }
        if self.pos >= self.data.len() || !self.data[self.pos].is_ascii_digit() {
            return fail(ErrorCode::Malformed);
        }
        if self.data[self.pos] == b'0' {
            self.pos += 1;
        } else {
            while self.pos < self.data.len() && self.data[self.pos].is_ascii_digit() {
                self.pos += 1;
            }
        }
        if self.pos < self.data.len() && self.data[self.pos] == b'.' {
            self.pos += 1;
            if self.pos >= self.data.len() || !self.data[self.pos].is_ascii_digit() {
                return fail(ErrorCode::Malformed);
            }
            while self.pos < self.data.len() && self.data[self.pos].is_ascii_digit() {
                self.pos += 1;
            }
        }
        if self.pos < self.data.len() && (self.data[self.pos] == b'e' || self.data[self.pos] == b'E') {
            self.pos += 1;
            if self.pos < self.data.len() && (self.data[self.pos] == b'+' || self.data[self.pos] == b'-') {
                self.pos += 1;
            }
            if self.pos >= self.data.len() || !self.data[self.pos].is_ascii_digit() {
                return fail(ErrorCode::Malformed);
            }
            while self.pos < self.data.len() && self.data[self.pos].is_ascii_digit() {
                self.pos += 1;
            }
        }
        let token = std::str::from_utf8(&self.data[start..self.pos]).map_err(|_| Error { code: ErrorCode::Malformed })?;
        let val: f64 = token.parse().map_err(|_| Error { code: ErrorCode::Number })?;
        validate_number(val)?;
        if val == 0.0 {
            let mantissa = token.split(['e', 'E']).next().unwrap_or(token);
            if mantissa.bytes().any(|b| (b'1'..=b'9').contains(&b)) {
                return fail(ErrorCode::Number);
            }
        }
        Ok(val)
    }

    fn parse_value(&mut self, depth: usize) -> Result<Value> {
        self.skip_ws();
        if self.pos >= self.data.len() {
            return fail(ErrorCode::Malformed);
        }
        if depth > self.limits.max_depth || self.items >= self.limits.max_items {
            return fail(ErrorCode::Limit);
        }
        self.items += 1;

        let c = self.data[self.pos];
        if c == b'{' {
            self.pos += 1;
            let mut obj = BTreeMap::new();
            self.skip_ws();
            if self.pos < self.data.len() && self.data[self.pos] == b'}' {
                self.pos += 1;
                return Ok(Value::Object(obj));
            }
            loop {
                self.skip_ws();
                if self.pos >= self.data.len() || self.data[self.pos] != b'"' {
                    return fail(ErrorCode::Malformed);
                }
                let key = self.parse_string()?;
                if obj.contains_key(&key) {
                    return fail(ErrorCode::DuplicateKey);
                }
                self.skip_ws();
                if self.pos >= self.data.len() || self.data[self.pos] != b':' {
                    return fail(ErrorCode::Malformed);
                }
                self.pos += 1; // consume ':'
                let val = self.parse_value(depth + 1)?;
                obj.insert(key, val);
                self.skip_ws();
                if self.pos >= self.data.len() {
                    return fail(ErrorCode::Malformed);
                }
                if self.data[self.pos] == b'}' {
                    self.pos += 1;
                    return Ok(Value::Object(obj));
                }
                if self.data[self.pos] != b',' {
                    return fail(ErrorCode::Malformed);
                }
                self.pos += 1; // consume ','
            }
        }

        if c == b'[' {
            self.pos += 1;
            let mut arr = Vec::new();
            self.skip_ws();
            if self.pos < self.data.len() && self.data[self.pos] == b']' {
                self.pos += 1;
                return Ok(Value::Array(arr));
            }
            loop {
                let val = self.parse_value(depth + 1)?;
                arr.push(val);
                self.skip_ws();
                if self.pos >= self.data.len() {
                    return fail(ErrorCode::Malformed);
                }
                if self.data[self.pos] == b']' {
                    self.pos += 1;
                    return Ok(Value::Array(arr));
                }
                if self.data[self.pos] != b',' {
                    return fail(ErrorCode::Malformed);
                }
                self.pos += 1; // consume ','
            }
        }

        if c == b'"' {
            return Ok(Value::String(self.parse_string()?));
        }

        let remaining = &self.data[self.pos..];
        if remaining.starts_with(b"NaN") || remaining.starts_with(b"Infinity") || remaining.starts_with(b"-Infinity") {
            return fail(ErrorCode::Number);
        }

        if remaining.starts_with(b"true") {
            self.pos += 4;
            return Ok(Value::Bool(true));
        }
        if remaining.starts_with(b"false") {
            self.pos += 5;
            return Ok(Value::Bool(false));
        }
        if remaining.starts_with(b"null") {
            self.pos += 4;
            return Ok(Value::Null);
        }

        if c == b'-' || c.is_ascii_digit() {
            return Ok(Value::Number(self.parse_number()?));
        }

        fail(ErrorCode::Malformed)
    }
}
