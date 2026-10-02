use std::io::{self, Read, Write};
use equorus::integrity::{
    canonical_bytes, compute_integrity, decode_integrity, encode_integrity, verify_integrity,
    CANONICAL_PROFILE, INTEGRITY_ALGORITHM,
};
use equorus::limits::Limits;
use equorus::parser::parse_json;
use equorus::pilot::{create_pilot, decode_pilot};
use equorus::value::Value;
use equorus::writer::encode_json;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 3 && args.len() != 7 {
        std::process::exit(2);
    }

    let mut limits = Limits::default();
    if args.len() == 7 {
        limits.max_bytes = match args[3].parse() {
            Ok(v) => v,
            Err(_) => std::process::exit(2),
        };
        limits.max_depth = match args[4].parse() {
            Ok(v) => v,
            Err(_) => std::process::exit(2),
        };
        limits.max_items = match args[5].parse() {
            Ok(v) => v,
            Err(_) => std::process::exit(2),
        };
        limits.max_string_length = match args[6].parse() {
            Ok(v) => v,
            Err(_) => std::process::exit(2),
        };
    }

    let mode = &args[1];
    let arg = &args[2];

    let mut raw = Vec::new();
    if let Err(e) = io::stdin().read_to_end(&mut raw) {
        eprintln!("read error: {}", e);
        std::process::exit(2);
    }

    if raw.len() > limits.max_bytes {
        print!("ERROR {}", equorus::ErrorCode::Limit.as_str());
        std::process::exit(1);
    }

    match mode.as_str() {
        "canonical" => {
            let val = match parse_json(&raw, &limits) {
                Ok(v) => v,
                Err(e) => handle_err(e),
            };
            let canon = match canonical_bytes(&val, arg, &limits) {
                Ok(b) => b,
                Err(e) => handle_err(e),
            };
            let _ = io::stdout().write_all(&canon);
        }

        "hash" => {
            let env = match decode_pilot(&raw, arg, &limits) {
                Ok(e) => e,
                Err(e) => handle_err(e),
            };
            let rec = match compute_integrity(&env, CANONICAL_PROFILE, INTEGRITY_ALGORITHM, &limits) {
                Ok(r) => r,
                Err(e) => handle_err(e),
            };
            let enc = match encode_integrity(&rec, &limits) {
                Ok(b) => b,
                Err(e) => handle_err(e),
            };
            let _ = io::stdout().write_all(&enc);
        }

        "record" => {
            let rec = match decode_integrity(&raw, &limits) {
                Ok(r) => r,
                Err(e) => handle_err(e),
            };
            let enc = match encode_integrity(&rec, &limits) {
                Ok(b) => b,
                Err(e) => handle_err(e),
            };
            let _ = io::stdout().write_all(&enc);
        }

        "verify" => {
            let req_val = match parse_json(&raw, &limits) {
                Ok(v) => v,
                Err(e) => handle_err(e),
            };
            let obj = match req_val {
                Value::Object(m) => m,
                _ => {
                    print!("ERROR {}", equorus::ErrorCode::Schema.as_str());
                    std::process::exit(1);
                }
            };
            let env_val = match obj.get("envelope") {
                Some(v) => v.clone(),
                None => {
                    print!("ERROR {}", equorus::ErrorCode::Schema.as_str());
                    std::process::exit(1);
                }
            };
            let int_val = match obj.get("integrity") {
                Some(v) => v,
                None => {
                    print!("ERROR {}", equorus::ErrorCode::Schema.as_str());
                    std::process::exit(1);
                }
            };
            let env = match create_pilot(env_val, arg, &limits) {
                Ok(e) => e,
                Err(e) => handle_err(e),
            };
            let int_bytes = match encode_json(int_val, &limits) {
                Ok(b) => b,
                Err(e) => handle_err(e),
            };
            let rec = match decode_integrity(&int_bytes, &limits) {
                Ok(r) => r,
                Err(e) => handle_err(e),
            };
            let ok = match verify_integrity(&env, &rec, &limits) {
                Ok(b) => b,
                Err(e) => handle_err(e),
            };
            if ok {
                print!("true");
            } else {
                print!("false");
            }
        }

        _ => std::process::exit(2),
    }
}

fn handle_err(e: equorus::Error) -> ! {
    print!("ERROR {}", e.code.as_str());
    std::process::exit(1);
}
