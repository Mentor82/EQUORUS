package equorus

import (
	"bytes"
	"math"
	"regexp"
	"strconv"
)

var (
	timeRegex = regexp.MustCompile(`^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}(\.[0-9]{1,6})?(Z|[+-][0-9]{2}:[0-9]{2})$`)
	supportedVersions = []string{"0.1"}
)

func asObject(v Value) map[string]Value {
	if v.Kind != KindObject {
		fail(ErrSchema)
	}
	return v.Object
}

func asArray(v Value) []Value {
	if v.Kind != KindArray {
		fail(ErrSchema)
	}
	return v.Array
}

func asString(v Value) string {
	if v.Kind != KindString {
		fail(ErrSchema)
	}
	return v.String
}

func asNumber(v Value) float64 {
	if v.Kind != KindNumber {
		fail(ErrSchema)
	}
	return v.Number
}

func asBool(v Value) bool {
	if v.Kind != KindBool {
		fail(ErrSchema)
	}
	return v.Bool
}

func checkKeys(o map[string]Value, required []string, optional []string) {
	for _, r := range required {
		if _, ok := o[r]; !ok {
			fail(ErrSchema)
		}
	}
	for k := range o {
		found := false
		for _, r := range required {
			if k == r {
				found = true
				break
			}
		}
		if !found && optional != nil {
			for _, opt := range optional {
				if k == opt {
					found = true
					break
				}
			}
		}
		if !found {
			fail(ErrSchema)
		}
	}
}

func checkOneOf(v Value, choices []string) {
	s := asString(v)
	for _, c := range choices {
		if s == c {
			return
		}
	}
	fail(ErrSchema)
}

func checkEqual(v Value, expected string) {
	if asString(v) != expected {
		fail(ErrSchema)
	}
}

func checkInteger(v Value, low, high float64) {
	n := asNumber(v)
	if math.Trunc(n) != n || n < low || n > high {
		fail(ErrSchema)
	}
}

func checkRatio(v Value) {
	n := asNumber(v)
	if n < 0 || n > 1 {
		fail(ErrSchema)
	}
}

func checkBoundedID(v Value) {
	n := UnicodeLength(asString(v))
	if n == 0 || n > 128 {
		fail(ErrSchema)
	}
}

func checkF32(v Value) {
	n := asNumber(v)
	if math.Abs(n) > math.MaxFloat32 || float64(float32(n)) != n {
		fail(ErrFloat32)
	}
}

func checkU64(v Value, nonzero bool) {
	ParseUint64(asString(v), nonzero)
}

func checkTimestamp(v Value) {
	s := asString(v)
	if !timeRegex.MatchString(s) {
		fail(ErrSchema)
	}
	digits := func(pos, n int) int {
		res, _ := strconv.Atoi(s[pos : pos+n])
		return res
	}
	year := digits(0, 4)
	month := digits(5, 2)
	day := digits(8, 2)
	if year < 1 || month < 1 || month > 12 {
		fail(ErrTimestamp)
	}
	leap := (year%4 == 0 && (year%100 != 0 || year%400 == 0))
	days := []int{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
	if leap {
		days[1] = 29
	}
	if day < 1 || day > days[month-1] || digits(11, 2) > 23 || digits(14, 2) > 59 || digits(17, 2) > 59 {
		fail(ErrTimestamp)
	}
	if s[len(s)-1] != 'Z' {
		offset := len(s) - 6
		if digits(offset+1, 2) > 23 || digits(offset+4, 2) > 59 {
			fail(ErrTimestamp)
		}
	}
}

func validateProvenance(v Value) {
	o := asObject(v)
	checkKeys(o, []string{"kind"}, []string{"source_id", "timestamp_ms"})
	checkOneOf(o["kind"], []string{"SOURCE_LITERAL", "TOOL_EVIDENCE", "MODEL_GENERATED", "DERIVED_CONTEXT"})
	if sid, ok := o["source_id"]; ok && sid.Kind != KindNull {
		asString(sid)
	}
	if ts, ok := o["timestamp_ms"]; ok {
		checkU64(ts, false)
	}
}

var metricUnits = map[string]string{
	"utilization_ratio":        "ratio",
	"memory_used_ratio":        "ratio",
	"temperature_c":            "celsius",
	"power_w":                  "watts",
	"charge_ratio":             "ratio",
	"charge_rate_w":            "watts",
	"external_power_connected": "boolean",
	"queue_depth":              "count",
	"active_work":              "count",
	"available":                "boolean",
}

func validateObservation(v Value) {
	o := asObject(v)
	checkKeys(o, []string{"resource", "metric", "value", "unit", "device_id", "observed_at", "source_id", "confidence", "attributes"}, nil)
	checkOneOf(o["resource"], []string{"cpu", "ram", "gpu", "npu", "battery", "thermal", "power", "system"})
	checkOneOf(o["unit"], []string{"ratio", "celsius", "watts", "count", "boolean"})
	metricName := asString(o["metric"])
	expectedUnit, ok := metricUnits[metricName]
	if !ok {
		fail(ErrSchema)
	}
	unit := asString(o["unit"])
	checkBoundedID(o["device_id"])
	checkBoundedID(o["source_id"])
	checkTimestamp(o["observed_at"])
	checkRatio(o["confidence"])
	if len(asObject(o["attributes"])) != 0 {
		fail(ErrSchema)
	}
	n := asNumber(o["value"])
	if expectedUnit != unit {
		fail(ErrMetricUnit)
	}
	if (unit == "ratio" && (n < 0 || n > 1)) || (unit == "count" && n < 0) || (unit == "boolean" && n != 0 && n != 1) {
		fail(ErrMetricValue)
	}
}

func validateHeartbeat(v Value) {
	o := asObject(v)
	checkKeys(o, []string{"schema_version", "instance_id", "instance_type", "node_id", "sequence", "observed_at", "state", "observations", "signals", "confidence"}, nil)
	checkEqual(o["schema_version"], "1.0")
	checkEqual(o["instance_type"], "heartbeat")
	asString(o["instance_id"])
	asString(o["node_id"])
	checkU64(o["sequence"], false)
	checkOneOf(o["state"], []string{"healthy", "constrained", "degraded", "critical", "unknown"})
	checkTimestamp(o["observed_at"])
	checkRatio(o["confidence"])
	for _, x := range asArray(o["signals"]) {
		asString(x)
	}
	for _, x := range asArray(o["observations"]) {
		validateObservation(x)
	}
}

func validateOptions(v Value) {
	o := asObject(v)
	checkKeys(o, []string{"top_p", "top_k", "repeat_penalty", "repeat_last_n", "seed", "presence_penalty", "frequency_penalty", "stop_sequences", "extra_options"}, nil)
	for _, name := range []string{"top_p", "repeat_penalty", "presence_penalty", "frequency_penalty"} {
		checkF32(o[name])
	}
	for _, name := range []string{"top_k", "repeat_last_n"} {
		checkInteger(o[name], -2147483648.0, 2147483647.0)
	}
	checkU64(o["seed"], false)
	for _, x := range asArray(o["stop_sequences"]) {
		asString(x)
	}
	var last []byte
	first := true
	for _, x := range asArray(o["extra_options"]) {
		pair := asArray(x)
		if len(pair) != 2 {
			fail(ErrSchema)
		}
		key := []byte(asString(pair[0]))
		asString(pair[1])
		if !first && bytes.Compare(last, key) >= 0 {
			fail(ErrOptionKeys)
		}
		first = false
		last = key
	}
}

func validateRequest(v Value) {
	o := asObject(v)
	checkKeys(o, []string{"protocol_version", "stream", "profile", "model_id", "payload", "max_tokens", "temperature", "stream_requested", "has_options"}, []string{"options"})
	checkEqual(o["protocol_version"], "0.2")
	checkOneOf(o["profile"], []string{"generate", "chat", "embed"})
	if asString(o["model_id"]) == "" {
		fail(ErrSchema)
	}
	asString(o["payload"])
	asBool(o["stream_requested"])
	checkInteger(o["max_tokens"], 0, 4294967295.0)
	stream := asObject(o["stream"])
	checkKeys(stream, []string{"request_id", "execution_id", "output_id"}, nil)
	checkU64(stream["request_id"], true)
	checkU64(stream["execution_id"], true)
	checkInteger(stream["output_id"], 0, 4294967295.0)
	checkF32(o["temperature"])
	has := asBool(o["has_options"])
	_, hasOptField := o["options"]
	if has != hasOptField {
		fail(ErrSchema)
	}
	if has {
		validateOptions(o["options"])
	}
}

func ValidatePilot(root Value) {
	o := asObject(root)
	checkKeys(o, []string{"type_id", "schema_version", "provenance", "payload"}, nil)
	typeID := asString(o["type_id"])
	if typeID != "vinox.provenance.snapshot" && typeID != "liara.heartbeat.snapshot" && typeID != "linep.v02.request" {
		fail(ErrType)
	}
	checkEqual(o["schema_version"], "0.1")
	validateProvenance(o["provenance"])
	if typeID == "vinox.provenance.snapshot" {
		if len(asObject(o["payload"])) != 0 {
			fail(ErrSchema)
		}
	} else if typeID == "liara.heartbeat.snapshot" {
		validateHeartbeat(o["payload"])
	} else {
		validateRequest(o["payload"])
	}
}

func DecodePilot(bytes []byte, expected string, limits Limits) (Envelope, error) {
	return DecodeEnvelope(JsonCodec{}, bytes, expected, supportedVersions, ValidatePilot, limits)
}

func CreatePilot(val Value, expected string, limits Limits) (Envelope, error) {
	return CreateEnvelope(val, expected, supportedVersions, ValidatePilot, limits)
}
