// EQUORUS Studio - Presets and Conformance Test Cases

export const PRESETS = [
  {
    id: 'vinox-full',
    name: 'VINOX: Full Provenance',
    category: 'VINOX',
    typeId: 'vinox.provenance.snapshot',
    expectedDigest: '923c69729c0953d74efe0d297802f005f89e5560869a37a5b75c7206b5d43ccd',
    json: `{
  "type_id": "vinox.provenance.snapshot",
  "schema_version": "0.1",
  "provenance": {
    "kind": "TOOL_EVIDENCE",
    "source_id": "fixture:vinox/tool/α",
    "timestamp_ms": "18446744073709551615"
  },
  "payload": {}
}`
  },
  {
    id: 'vinox-minimal',
    name: 'VINOX: Minimal Snapshot',
    category: 'VINOX',
    typeId: 'vinox.provenance.snapshot',
    expectedDigest: 'a0cb589bd78c4aed886a693f7858540523ae6d4da50c88b5689652e26bb19546',
    json: `{
  "type_id": "vinox.provenance.snapshot",
  "schema_version": "0.1",
  "provenance": {
    "kind": "SOURCE_LITERAL"
  },
  "payload": {}
}`
  },
  {
    id: 'vinox-null-source',
    name: 'VINOX: Null Source ID',
    category: 'VINOX',
    typeId: 'vinox.provenance.snapshot',
    expectedDigest: '9095c64165127126fb09484db2a0a30f62691c891ee59dfb5f07323b17278579',
    json: `{
  "type_id": "vinox.provenance.snapshot",
  "schema_version": "0.1",
  "provenance": {
    "kind": "DERIVED_CONTEXT",
    "source_id": null,
    "timestamp_ms": "0"
  },
  "payload": {}
}`
  },
  {
    id: 'liara-heartbeat',
    name: 'LIARA: Heartbeat Snapshot',
    category: 'LIARA',
    typeId: 'liara.heartbeat.snapshot',
    expectedDigest: '68a469a10d89a6367b54058cb95e262088abf4ec76452bdbc4bb2bf399a5d196',
    json: `{
  "type_id": "liara.heartbeat.snapshot",
  "schema_version": "0.1",
  "provenance": {
    "kind": "SOURCE_LITERAL",
    "source_id": "fixture:synthetic"
  },
  "payload": {
    "schema_version": "1.0",
    "instance_id": "fixture-heartbeat",
    "instance_type": "heartbeat",
    "node_id": "fixture-node",
    "sequence": "9007199254740993",
    "observed_at": "2026-10-01T12:00:00.123456+02:00",
    "state": "healthy",
    "observations": [
      {
        "resource": "cpu",
        "metric": "utilization_ratio",
        "value": 0.5,
        "unit": "ratio",
        "device_id": "default",
        "observed_at": "2026-10-01T12:00:00.123456+02:00",
        "source_id": "fixture:sensor",
        "confidence": 1,
        "attributes": {}
      }
    ],
    "signals": [],
    "confidence": 0.75
  }
}`
  },
  {
    id: 'linep-options',
    name: 'LiNeP v0.2: Request (with Options)',
    category: 'LiNeP',
    typeId: 'linep.v02.request',
    expectedDigest: '343e28b85d1d3c139bbb26e13febf4d646db12c15caad0cb9b96b01b8a4644f9',
    json: `{
  "type_id": "linep.v02.request",
  "schema_version": "0.1",
  "provenance": {
    "kind": "SOURCE_LITERAL",
    "source_id": "fixture:synthetic"
  },
  "payload": {
    "protocol_version": "0.2",
    "stream": {
      "request_id": "9007199254740993",
      "execution_id": "18446744073709551615",
      "output_id": 0
    },
    "profile": "chat",
    "model_id": "fixture/model",
    "payload": "{\\"messages\\":[{\\"role\\":\\"user\\",\\"content\\":\\"Grüße 🌙\\"}]}",
    "max_tokens": 64,
    "temperature": 0.5,
    "stream_requested": true,
    "has_options": true,
    "options": {
      "top_p": 0.8999999761581421,
      "top_k": 40,
      "repeat_penalty": 1,
      "repeat_last_n": 64,
      "seed": "18446744073709551615",
      "presence_penalty": 0,
      "frequency_penalty": 0,
      "stop_sequences": [
        "END",
        "STOP"
      ],
      "extra_options": [
        [
          "a",
          "1"
        ],
        [
          "z",
          "2"
        ]
      ]
    }
  }
}`
  },
  {
    id: 'linep-no-options',
    name: 'LiNeP v0.2: Request (no Options)',
    category: 'LiNeP',
    typeId: 'linep.v02.request',
    expectedDigest: 'b8c34b7cf7c6fed4e5e7baf9e6d0bf18ee1ac8c82147e8f89703ad3352b82832',
    json: `{
  "type_id": "linep.v02.request",
  "schema_version": "0.1",
  "provenance": {
    "kind": "SOURCE_LITERAL",
    "source_id": "fixture:synthetic"
  },
  "payload": {
    "protocol_version": "0.2",
    "stream": {
      "request_id": "9007199254740993",
      "execution_id": "18446744073709551615",
      "output_id": 0
    },
    "profile": "chat",
    "model_id": "fixture/model",
    "payload": "{\\"messages\\":[{\\"role\\":\\"user\\",\\"content\\":\\"Grüße 🌙\\"}]}",
    "max_tokens": 64,
    "temperature": 0.5,
    "stream_requested": true,
    "has_options": false
  }
}`
  },
  {
    id: 'adv-lone-surrogate',
    name: 'Adversarial: Lone Surrogate (Unicode Error)',
    category: 'Adversarial',
    typeId: 'vinox.provenance.snapshot',
    expectedDigest: null,
    json: `{
  "type_id": "vinox.provenance.snapshot",
  "schema_version": "0.1",
  "provenance": {
    "kind": "SOURCE_LITERAL",
    "source_id": "\\ud800"
  },
  "payload": {}
}`
  },
  {
    id: 'adv-duplicate-key',
    name: 'Adversarial: Duplicate Key',
    category: 'Adversarial',
    typeId: 'vinox.provenance.snapshot',
    expectedDigest: null,
    json: `{
  "type_id": "vinox.provenance.snapshot",
  "type_id": "vinox.provenance.snapshot",
  "schema_version": "0.1",
  "provenance": {
    "kind": "SOURCE_LITERAL"
  },
  "payload": {}
}`
  },
  {
    id: 'adv-neg-zero',
    name: 'Adversarial: Negative Zero (-0.0)',
    category: 'Adversarial',
    typeId: 'vinox.provenance.snapshot',
    expectedDigest: null,
    json: `{
  "type_id": "vinox.provenance.snapshot",
  "schema_version": "0.1",
  "provenance": {
    "kind": "SOURCE_LITERAL"
  },
  "payload": {
    "bad_value": -0.0
  }
}`
  },
  {
    id: 'adv-liara-attributes',
    name: 'Adversarial: Non-Empty LIARA Attributes',
    category: 'Adversarial',
    typeId: 'liara.heartbeat.snapshot',
    expectedDigest: null,
    json: `{
  "type_id": "liara.heartbeat.snapshot",
  "schema_version": "0.1",
  "provenance": {
    "kind": "SOURCE_LITERAL",
    "source_id": "fixture:synthetic"
  },
  "payload": {
    "sequence": "100",
    "observations": [
      {
        "resource": "cpu",
        "attributes": {
          "illegal_extra": "forbidden_in_v0.1"
        }
      }
    ]
  }
}`
  }
];
