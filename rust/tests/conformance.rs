use equorus::integrity::{
    canonical_bytes, compute_integrity, verify_integrity, CANONICAL_PROFILE, INTEGRITY_ALGORITHM,
};
use equorus::limits::Limits;
use equorus::linep::{from_linep_request, to_linep_request};
use equorus::parser::parse_json;
use equorus::pilot::decode_pilot;
use equorus::sha256::Sha256;
use equorus::value::Value;
use std::fs;
use std::path::PathBuf;

fn fixtures_dir() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .unwrap()
        .join("tests")
        .join("fixtures")
}

#[test]
fn test_sha256_known_answers() {
    assert_eq!(
        Sha256::digest_hex(b""),
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
    );
    assert_eq!(
        Sha256::digest_hex(b"abc"),
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
    );
    assert_eq!(
        Sha256::digest_hex(b"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"
    );
}

fn hex_to_bytes(s: &str) -> Vec<u8> {
    let mut bytes = Vec::with_capacity(s.len() / 2);
    let mut chars = s.chars();
    while let (Some(a), Some(b)) = (chars.next(), chars.next()) {
        let hi = a.to_digit(16).unwrap() as u8;
        let lo = b.to_digit(16).unwrap() as u8;
        bytes.push((hi << 4) | lo);
    }
    bytes
}

#[test]
fn test_canonical_vectors_from_file() {
    let limits = Limits {
        max_bytes: 100_000,
        ..Limits::default()
    };
    let vec_raw = fs::read(fixtures_dir().join("canonical-v1").join("vectors.json")).unwrap();
    let vec_json = parse_json(&vec_raw, &limits).unwrap();

    let obj = match vec_json {
        Value::Object(m) => m,
        _ => panic!("vectors.json is not an object"),
    };

    let profile = match obj.get("profile") {
        Some(Value::String(s)) => s.as_str(),
        _ => panic!("missing profile"),
    };
    assert_eq!(profile, CANONICAL_PROFILE);

    // Test values
    if let Some(Value::Array(values)) = obj.get("values") {
        for v in values {
            if let Value::Object(v_obj) = v {
                let name = match v_obj.get("name") {
                    Some(Value::String(s)) => s.as_str(),
                    _ => "",
                };
                let raw_json = match v_obj.get("json") {
                    Some(Value::String(s)) => s.as_bytes(),
                    _ => panic!("missing json for value"),
                };
                let hex_str = match v_obj.get("hex") {
                    Some(Value::String(s)) => s.as_str(),
                    _ => panic!("missing hex for value"),
                };
                let expected = hex_to_bytes(hex_str);
                let parsed_val = parse_json(raw_json, &limits).unwrap();
                let actual = canonical_bytes(&parsed_val, profile, &limits).unwrap();
                assert_eq!(actual, expected, "Mismatch for value vector {}", name);
            }
        }
    }

    // Test envelopes
    if let Some(Value::Array(envelopes)) = obj.get("envelopes") {
        let pilot_dir = fixtures_dir().join("pilot-v0.1");
        for env_entry in envelopes {
            if let Value::Object(e_obj) = env_entry {
                let file = match e_obj.get("file") {
                    Some(Value::String(s)) => s.as_str(),
                    _ => panic!("missing file"),
                };
                let expected_hex = match e_obj.get("hex") {
                    Some(Value::String(s)) => s.as_str(),
                    _ => panic!("missing hex"),
                };
                let expected_digest = match e_obj.get("digest") {
                    Some(Value::String(s)) => s.as_str(),
                    _ => panic!("missing digest"),
                };

                let raw = fs::read(pilot_dir.join(file)).unwrap();
                let parsed_val = parse_json(&raw, &limits).unwrap();
                let type_id = match &parsed_val {
                    Value::Object(m) => match m.get("type_id") {
                        Some(Value::String(s)) => s.clone(),
                        _ => panic!("missing type_id"),
                    },
                    _ => panic!("envelope not object"),
                };

                let env = decode_pilot(&raw, &type_id, &limits).unwrap();
                let canon = canonical_bytes(env.value(), profile, &limits).unwrap();
                let expected_bytes = hex_to_bytes(expected_hex);
                assert_eq!(canon, expected_bytes, "Canonical bytes mismatch for {}", file);

                let rec = compute_integrity(&env, profile, INTEGRITY_ALGORITHM, &limits).unwrap();
                assert_eq!(rec.digest, expected_digest, "Digest mismatch for {}", file);

                let ok = verify_integrity(&env, &rec, &limits).unwrap();
                assert!(ok, "Integrity verification failed for {}", file);
            }
        }
    }
}

#[test]
fn test_linep_adapter_roundtrip() {
    let limits = Limits::default();
    let pilot_dir = fixtures_dir().join("pilot-v0.1");

    for file in &["linep-options.json", "linep-no-options.json"] {
        let raw = fs::read(pilot_dir.join(file)).unwrap();
        let env = decode_pilot(&raw, "linep.v02.request", &limits).unwrap();

        let req = to_linep_request(&env).unwrap();
        let prov = match env.value() {
            Value::Object(m) => match m.get("provenance") {
                Some(Value::Object(p)) => Some(p.clone()),
                _ => None,
            },
            _ => None,
        };

        let env2 = from_linep_request(&req, prov, &limits).unwrap();

        let c1 = canonical_bytes(env.value(), CANONICAL_PROFILE, &limits).unwrap();
        let c2 = canonical_bytes(env2.value(), CANONICAL_PROFILE, &limits).unwrap();
        assert_eq!(c1, c2, "Roundtrip canonical bytes mismatch for {}", file);
    }
}

#[test]
fn test_error_rejections() {
    let limits = Limits::default();

    // Lone surrogate
    let res = parse_json(br#"{"a":"\ud800"}"#, &limits);
    assert_eq!(res.unwrap_err().code, equorus::ErrorCode::Unicode);

    // Duplicate key
    let res = parse_json(br#"{"a":1,"a":2}"#, &limits);
    assert_eq!(res.unwrap_err().code, equorus::ErrorCode::DuplicateKey);

    // Negative zero
    let res = parse_json(br#"{"a":-0.0}"#, &limits);
    assert_eq!(res.unwrap_err().code, equorus::ErrorCode::Number);

    // NaN
    let res = parse_json(br#"{"a":NaN}"#, &limits);
    assert_eq!(res.unwrap_err().code, equorus::ErrorCode::Number);

    // Trailing bytes
    let res = parse_json(br#"{"a":1} trailing"#, &limits);
    assert_eq!(res.unwrap_err().code, equorus::ErrorCode::Malformed);

    // Depth limit
    let strict_depth = Limits { max_depth: 2, ..Limits::default() };
    let res = parse_json(br#"{"a":{"b":{"c":1}}}"#, &strict_depth);
    assert_eq!(res.unwrap_err().code, equorus::ErrorCode::Limit);
}
