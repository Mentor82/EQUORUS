use crate::error::{fail, ErrorCode, Result};
use crate::limits::Limits;
use crate::parser::parse_json;
use crate::value::{validate_value, Value};
use crate::writer::encode_json;

#[derive(Debug, Clone, PartialEq)]
pub struct Envelope {
    root: Value,
}

fn field<'a>(root: &'a Value, name: &str, code: ErrorCode) -> Result<&'a str> {
    match root {
        Value::Object(obj) => match obj.get(name) {
            Some(Value::String(s)) => Ok(s.as_str()),
            _ => fail(code),
        },
        _ => fail(ErrorCode::Schema),
    }
}

impl Envelope {
    pub fn create(
        root: Value,
        expected: &str,
        versions: &[&str],
        validator: impl Fn(&Value) -> Result<()>,
        limits: &Limits,
    ) -> Result<Self> {
        validate_value(&root, limits)?;
        if expected.is_empty() || field(&root, "type_id", ErrorCode::Type)? != expected {
            return fail(ErrorCode::Type);
        }
        let ver = field(&root, "schema_version", ErrorCode::Version)?;
        if !versions.contains(&ver) {
            return fail(ErrorCode::Version);
        }
        match &root {
            Value::Object(obj) => {
                if obj.len() != 4 || !obj.contains_key("payload") {
                    return fail(ErrorCode::Schema);
                }
                match obj.get("provenance") {
                    Some(Value::Object(_)) => {}
                    _ => return fail(ErrorCode::Schema),
                }
            }
            _ => return fail(ErrorCode::Schema),
        }
        validator(&root)?;
        Ok(Self { root })
    }

    pub fn decode(
        raw: &[u8],
        expected: &str,
        versions: &[&str],
        validator: impl Fn(&Value) -> Result<()>,
        limits: &Limits,
    ) -> Result<Self> {
        let val = parse_json(raw, limits)?;
        Self::create(val, expected, versions, validator, limits)
    }

    pub fn value(&self) -> &Value {
        &self.root
    }

    pub fn type_id(&self) -> &str {
        field(&self.root, "type_id", ErrorCode::Type).unwrap()
    }

    pub fn schema_version(&self) -> &str {
        field(&self.root, "schema_version", ErrorCode::Version).unwrap()
    }

    pub fn encode(&self, limits: &Limits) -> Result<Vec<u8>> {
        encode_json(&self.root, limits)
    }
}
