use std::collections::BTreeMap;
use crate::envelope::Envelope;
use crate::error::{fail, ErrorCode, Result};
use crate::limits::Limits;
use crate::pilot::create_pilot;
use crate::value::{parse_uint64, Value};

pub mod profile {
    pub const UNSPECIFIED: u8 = 0;
    pub const GENERATE: u8 = 1;
    pub const CHAT: u8 = 2;
    pub const EMBED: u8 = 3;
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Hash)]
pub struct StreamIdentity {
    pub request_id: u64,
    pub execution_id: u64,
    pub output_id: u32,
}

#[derive(Debug, Clone, PartialEq)]
pub struct GenerationOptions {
    pub top_p: f32,
    pub top_k: i32,
    pub repeat_penalty: f32,
    pub repeat_last_n: i32,
    pub seed: u64,
    pub presence_penalty: f32,
    pub frequency_penalty: f32,
    pub stop_sequences: Vec<String>,
    pub extra_options: Vec<(String, String)>,
}

#[derive(Debug, Clone, PartialEq)]
pub struct RequestEnvelope {
    pub stream: StreamIdentity,
    pub profile: u8,
    pub model_id: String,
    pub payload: Vec<u8>,
    pub max_tokens: u32,
    pub temperature: f32,
    pub stream_requested: bool,
    pub options: Option<GenerationOptions>,
}

pub fn to_linep_request(env: &Envelope) -> Result<RequestEnvelope> {
    if env.type_id() != "linep.v02.request" {
        return fail(ErrorCode::Type);
    }
    let obj = match env.value() {
        Value::Object(m) => m,
        _ => return fail(ErrorCode::Schema),
    };
    let payload = match obj.get("payload") {
        Some(Value::Object(m)) => m,
        _ => return fail(ErrorCode::Schema),
    };

    let stream_obj = match payload.get("stream") {
        Some(Value::Object(m)) => m,
        _ => return fail(ErrorCode::Schema),
    };
    let req_id_str = match stream_obj.get("request_id") {
        Some(Value::String(s)) => s,
        _ => return fail(ErrorCode::Schema),
    };
    let exec_id_str = match stream_obj.get("execution_id") {
        Some(Value::String(s)) => s,
        _ => return fail(ErrorCode::Schema),
    };
    let out_id_num = match stream_obj.get("output_id") {
        Some(Value::Number(n)) => *n as u32,
        _ => return fail(ErrorCode::Schema),
    };

    let stream = StreamIdentity {
        request_id: parse_uint64(req_id_str, true)?,
        execution_id: parse_uint64(exec_id_str, true)?,
        output_id: out_id_num,
    };

    let prof_str = match payload.get("profile") {
        Some(Value::String(s)) => s.as_str(),
        _ => return fail(ErrorCode::Schema),
    };
    let prof = match prof_str {
        "generate" => profile::GENERATE,
        "chat" => profile::CHAT,
        "embed" => profile::EMBED,
        _ => return fail(ErrorCode::Schema),
    };

    let model_id = match payload.get("model_id") {
        Some(Value::String(s)) => s.clone(),
        _ => return fail(ErrorCode::Schema),
    };
    let req_payload = match payload.get("payload") {
        Some(Value::String(s)) => s.as_bytes().to_vec(),
        _ => return fail(ErrorCode::Schema),
    };
    let max_tokens = match payload.get("max_tokens") {
        Some(Value::Number(n)) => *n as u32,
        _ => return fail(ErrorCode::Schema),
    };
    let temperature = match payload.get("temperature") {
        Some(Value::Number(n)) => *n as f32,
        _ => return fail(ErrorCode::Schema),
    };
    let stream_requested = match payload.get("stream_requested") {
        Some(Value::Bool(b)) => *b,
        _ => return fail(ErrorCode::Schema),
    };
    let has_options = match payload.get("has_options") {
        Some(Value::Bool(b)) => *b,
        _ => return fail(ErrorCode::Schema),
    };

    let options = if has_options {
        let opt_obj = match payload.get("options") {
            Some(Value::Object(m)) => m,
            _ => return fail(ErrorCode::Schema),
        };
        let top_p = match opt_obj.get("top_p") {
            Some(Value::Number(n)) => *n as f32,
            _ => return fail(ErrorCode::Schema),
        };
        let top_k = match opt_obj.get("top_k") {
            Some(Value::Number(n)) => *n as i32,
            _ => return fail(ErrorCode::Schema),
        };
        let repeat_penalty = match opt_obj.get("repeat_penalty") {
            Some(Value::Number(n)) => *n as f32,
            _ => return fail(ErrorCode::Schema),
        };
        let repeat_last_n = match opt_obj.get("repeat_last_n") {
            Some(Value::Number(n)) => *n as i32,
            _ => return fail(ErrorCode::Schema),
        };
        let seed_str = match opt_obj.get("seed") {
            Some(Value::String(s)) => s,
            _ => return fail(ErrorCode::Schema),
        };
        let seed = parse_uint64(seed_str, false)?;
        let presence_penalty = match opt_obj.get("presence_penalty") {
            Some(Value::Number(n)) => *n as f32,
            _ => return fail(ErrorCode::Schema),
        };
        let frequency_penalty = match opt_obj.get("frequency_penalty") {
            Some(Value::Number(n)) => *n as f32,
            _ => return fail(ErrorCode::Schema),
        };
        let mut stop_sequences = Vec::new();
        if let Some(Value::Array(arr)) = opt_obj.get("stop_sequences") {
            for item in arr {
                if let Value::String(s) = item {
                    stop_sequences.push(s.clone());
                } else {
                    return fail(ErrorCode::Schema);
                }
            }
        }
        let mut extra_options = Vec::new();
        if let Some(Value::Array(arr)) = opt_obj.get("extra_options") {
            for item in arr {
                if let Value::Array(pair) = item {
                    if pair.len() == 2 {
                        if let (Value::String(k), Value::String(v)) = (&pair[0], &pair[1]) {
                            extra_options.push((k.clone(), v.clone()));
                        } else {
                            return fail(ErrorCode::Schema);
                        }
                    } else {
                        return fail(ErrorCode::Schema);
                    }
                } else {
                    return fail(ErrorCode::Schema);
                }
            }
        }
        Some(GenerationOptions {
            top_p,
            top_k,
            repeat_penalty,
            repeat_last_n,
            seed,
            presence_penalty,
            frequency_penalty,
            stop_sequences,
            extra_options,
        })
    } else {
        None
    };

    Ok(RequestEnvelope {
        stream,
        profile: prof,
        model_id,
        payload: req_payload,
        max_tokens,
        temperature,
        stream_requested,
        options,
    })
}

pub fn from_linep_request(
    req: &RequestEnvelope,
    provenance: Option<BTreeMap<String, Value>>,
    limits: &Limits,
) -> Result<Envelope> {
    let prof_str = match req.profile {
        profile::GENERATE => "generate",
        profile::CHAT => "chat",
        profile::EMBED => "embed",
        _ => return fail(ErrorCode::Schema),
    };

    let mut stream_obj = BTreeMap::new();
    stream_obj.insert("request_id".to_string(), Value::String(req.stream.request_id.to_string()));
    stream_obj.insert("execution_id".to_string(), Value::String(req.stream.execution_id.to_string()));
    stream_obj.insert("output_id".to_string(), Value::Number(req.stream.output_id as f64));

    let mut payload = BTreeMap::new();
    payload.insert("protocol_version".to_string(), Value::String("0.2".to_string()));
    payload.insert("stream".to_string(), Value::Object(stream_obj));
    payload.insert("profile".to_string(), Value::String(prof_str.to_string()));
    payload.insert("model_id".to_string(), Value::String(req.model_id.clone()));
    let payload_str = String::from_utf8(req.payload.clone()).map_err(|_| crate::error::Error { code: ErrorCode::Schema })?;
    payload.insert("payload".to_string(), Value::String(payload_str));
    payload.insert("max_tokens".to_string(), Value::Number(req.max_tokens as f64));
    payload.insert("temperature".to_string(), Value::Number(req.temperature as f64));
    payload.insert("stream_requested".to_string(), Value::Bool(req.stream_requested));
    payload.insert("has_options".to_string(), Value::Bool(req.options.is_some()));

    if let Some(opt) = &req.options {
        let mut opt_obj = BTreeMap::new();
        opt_obj.insert("top_p".to_string(), Value::Number(opt.top_p as f64));
        opt_obj.insert("top_k".to_string(), Value::Number(opt.top_k as f64));
        opt_obj.insert("repeat_penalty".to_string(), Value::Number(opt.repeat_penalty as f64));
        opt_obj.insert("repeat_last_n".to_string(), Value::Number(opt.repeat_last_n as f64));
        opt_obj.insert("seed".to_string(), Value::String(opt.seed.to_string()));
        opt_obj.insert("presence_penalty".to_string(), Value::Number(opt.presence_penalty as f64));
        opt_obj.insert("frequency_penalty".to_string(), Value::Number(opt.frequency_penalty as f64));
        opt_obj.insert(
            "stop_sequences".to_string(),
            Value::Array(opt.stop_sequences.iter().map(|s| Value::String(s.clone())).collect()),
        );
        let mut sorted_extra = opt.extra_options.clone();
        sorted_extra.sort_by(|a, b| a.0.as_bytes().cmp(b.0.as_bytes()));
        for w in sorted_extra.windows(2) {
            if w[0].0 == w[1].0 {
                return fail(ErrorCode::OptionKeys);
            }
        }
        opt_obj.insert(
            "extra_options".to_string(),
            Value::Array(
                sorted_extra
                    .into_iter()
                    .map(|(k, v)| Value::Array(vec![Value::String(k), Value::String(v)]))
                    .collect(),
            ),
        );
        payload.insert("options".to_string(), Value::Object(opt_obj));
    }

    let prov = if let Some(p) = provenance {
        p
    } else {
        let mut p = BTreeMap::new();
        p.insert("kind".to_string(), Value::String("SOURCE_LITERAL".to_string()));
        p.insert("source_id".to_string(), Value::String("linep.adapter".to_string()));
        p
    };

    let mut root = BTreeMap::new();
    root.insert("type_id".to_string(), Value::String("linep.v02.request".to_string()));
    root.insert("schema_version".to_string(), Value::String("0.1".to_string()));
    root.insert("provenance".to_string(), Value::Object(prov));
    root.insert("payload".to_string(), Value::Object(payload));

    create_pilot(Value::Object(root), "linep.v02.request", limits)
}
