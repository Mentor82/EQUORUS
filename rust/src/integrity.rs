use std::collections::BTreeMap;
use crate::envelope::Envelope;
use crate::error::{fail, ErrorCode, Result};
use crate::limits::Limits;
use crate::parser::parse_json;
use crate::sha256::Sha256;
use crate::value::{validate_value, Value};
use crate::writer::encode_json;

pub const CANONICAL_PROFILE: &str = "equorus-value-v1";
pub const INTEGRITY_ALGORITHM: &str = "sha-256";

const DOMAIN_PREFIX: &[u8] = b"EQUORUS-INTEGRITY\0v1\0";

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct IntegrityRecord {
    pub profile: String,
    pub algorithm: String,
    pub digest: String,
}

impl IntegrityRecord {
    pub fn validate(&self) -> Result<()> {
        if self.profile != CANONICAL_PROFILE {
            return fail(ErrorCode::Profile);
        }
        if self.algorithm != INTEGRITY_ALGORITHM {
            return fail(ErrorCode::Algorithm);
        }
        if self.digest.len() != 64 {
            return fail(ErrorCode::Integrity);
        }
        for b in self.digest.bytes() {
            if !((b'0'..=b'9').contains(&b) || (b'a'..=b'f').contains(&b)) {
                return fail(ErrorCode::Integrity);
            }
        }
        Ok(())
    }
}

struct CanonicalWriter<'a> {
    limits: &'a Limits,
    out: Vec<u8>,
}

impl<'a> CanonicalWriter<'a> {
    fn append_bytes(&mut self, b: &[u8]) -> Result<()> {
        if self.out.len() + b.len() > self.limits.max_bytes {
            return fail(ErrorCode::Limit);
        }
        self.out.extend_from_slice(b);
        Ok(())
    }

    fn tag(&mut self, c: u8) -> Result<()> {
        self.append_bytes(&[c])
    }

    fn u64(&mut self, n: u64) -> Result<()> {
        self.append_bytes(&n.to_be_bytes())
    }

    fn string(&mut self, s: &str) -> Result<()> {
        self.tag(b's')?;
        self.u64(s.len() as u64)?;
        self.append_bytes(s.as_bytes())
    }

    fn value(&mut self, v: &Value) -> Result<()> {
        match v {
            Value::Null => self.tag(b'n'),
            Value::Bool(true) => self.tag(b't'),
            Value::Bool(false) => self.tag(b'f'),
            Value::Number(n) => {
                self.tag(b'd')?;
                self.append_bytes(&n.to_bits().to_be_bytes())
            }
            Value::String(s) => self.string(s),
            Value::Array(arr) => {
                self.tag(b'a')?;
                self.u64(arr.len() as u64)?;
                for item in arr {
                    self.value(item)?;
                }
                Ok(())
            }
            Value::Object(obj) => {
                self.tag(b'o')?;
                self.u64(obj.len() as u64)?;
                for (k, val) in obj {
                    self.string(k)?;
                    self.value(val)?;
                }
                Ok(())
            }
        }
    }
}

pub fn canonical_bytes(value: &Value, profile: &str, limits: &Limits) -> Result<Vec<u8>> {
    if profile != CANONICAL_PROFILE {
        return fail(ErrorCode::Profile);
    }
    validate_value(value, limits)?;
    let mut writer = CanonicalWriter {
        limits,
        out: Vec::with_capacity(128),
    };
    writer.value(value)?;
    Ok(writer.out)
}

pub fn compute_integrity(
    envelope: &Envelope,
    profile: &str,
    algorithm: &str,
    limits: &Limits,
) -> Result<IntegrityRecord> {
    if profile != CANONICAL_PROFILE {
        return fail(ErrorCode::Profile);
    }
    if algorithm != INTEGRITY_ALGORITHM {
        return fail(ErrorCode::Algorithm);
    }
    let canon = canonical_bytes(envelope.value(), profile, limits)?;
    let mut h = Sha256::new();
    h.update(DOMAIN_PREFIX);
    h.update(profile.as_bytes());
    h.update(&[0]);
    h.update(algorithm.as_bytes());
    h.update(&[0]);
    h.update(&canon);
    let digest = Sha256::digest_hex(&{
        // Recompute with full stream
        let mut full = Vec::new();
        full.extend_from_slice(DOMAIN_PREFIX);
        full.extend_from_slice(profile.as_bytes());
        full.push(0);
        full.extend_from_slice(algorithm.as_bytes());
        full.push(0);
        full.extend_from_slice(&canon);
        full
    });
    Ok(IntegrityRecord {
        profile: profile.to_string(),
        algorithm: algorithm.to_string(),
        digest,
    })
}

pub fn verify_integrity(envelope: &Envelope, record: &IntegrityRecord, limits: &Limits) -> Result<bool> {
    record.validate()?;
    let actual = compute_integrity(envelope, &record.profile, &record.algorithm, limits)?;
    // Constant time compare
    let mut diff = 0u8;
    for (a, b) in actual.digest.bytes().zip(record.digest.bytes()) {
        diff |= a ^ b;
    }
    Ok(diff == 0 && actual.digest.len() == record.digest.len())
}

pub fn encode_integrity(record: &IntegrityRecord, limits: &Limits) -> Result<Vec<u8>> {
    record.validate()?;
    let mut obj = BTreeMap::new();
    obj.insert("canonical_profile".to_string(), Value::String(record.profile.clone()));
    obj.insert("algorithm".to_string(), Value::String(record.algorithm.clone()));
    obj.insert("digest".to_string(), Value::String(record.digest.clone()));
    encode_json(&Value::Object(obj), limits)
}

pub fn decode_integrity(raw: &[u8], limits: &Limits) -> Result<IntegrityRecord> {
    let val = parse_json(raw, limits)?;
    match val {
        Value::Object(obj) if obj.len() == 3 => {
            let cp = match obj.get("canonical_profile") {
                Some(Value::String(s)) => s.clone(),
                _ => return fail(ErrorCode::Integrity),
            };
            let alg = match obj.get("algorithm") {
                Some(Value::String(s)) => s.clone(),
                _ => return fail(ErrorCode::Integrity),
            };
            let dig = match obj.get("digest") {
                Some(Value::String(s)) => s.clone(),
                _ => return fail(ErrorCode::Integrity),
            };
            let rec = IntegrityRecord {
                profile: cp,
                algorithm: alg,
                digest: dig,
            };
            rec.validate()?;
            Ok(rec)
        }
        _ => fail(ErrorCode::Integrity),
    }
}
