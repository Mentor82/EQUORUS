use std::io::{self, Read, Write};
use equorus::limits::Limits;
use equorus::pilot::{create_pilot, decode_pilot};

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 2 && args.len() != 6 {
        std::process::exit(2);
    }

    let mut limits = Limits::default();
    if args.len() == 6 {
        limits.max_bytes = match args[2].parse() {
            Ok(v) => v,
            Err(_) => std::process::exit(2),
        };
        limits.max_depth = match args[3].parse() {
            Ok(v) => v,
            Err(_) => std::process::exit(2),
        };
        limits.max_items = match args[4].parse() {
            Ok(v) => v,
            Err(_) => std::process::exit(2),
        };
        limits.max_string_length = match args[5].parse() {
            Ok(v) => v,
            Err(_) => std::process::exit(2),
        };
    }

    let expected_type = &args[1];

    let mut raw = Vec::new();
    if let Err(e) = io::stdin().read_to_end(&mut raw) {
        eprintln!("read error: {}", e);
        std::process::exit(2);
    }

    if raw.len() > limits.max_bytes {
        print!("ERROR {}", equorus::ErrorCode::Limit.as_str());
        std::process::exit(1);
    }

    let env = match decode_pilot(&raw, expected_type, &limits) {
        Ok(e) => e,
        Err(e) => {
            print!("ERROR {}", e.code.as_str());
            std::process::exit(1);
        }
    };

    let snapshot = match create_pilot(env.value().clone(), expected_type, &limits) {
        Ok(s) => s,
        Err(e) => {
            print!("ERROR {}", e.code.as_str());
            std::process::exit(1);
        }
    };

    let encoded = match snapshot.encode(&limits) {
        Ok(b) => b,
        Err(e) => {
            print!("ERROR {}", e.code.as_str());
            std::process::exit(1);
        }
    };

    let _ = io::stdout().write_all(&encoded);
}
