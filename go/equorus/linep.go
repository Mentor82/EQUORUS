package equorus

import (
	"sort"
	"strconv"
)

type RuntimeProfile uint8

const (
	ProfileUnspecified RuntimeProfile = 0
	ProfileGenerate    RuntimeProfile = 1
	ProfileChat        RuntimeProfile = 2
	ProfileEmbed       RuntimeProfile = 3
)

type StreamIdentity struct {
	RequestID   uint64
	ExecutionID uint64
	OutputID    uint32
}

type GenerationOptions struct {
	TopP             float32
	TopK             int32
	RepeatPenalty    float32
	RepeatLastN      int32
	Seed             uint64
	PresencePenalty  float32
	FrequencyPenalty float32
	StopSequences    []string
	ExtraOptions     [][2]string
}

type LinePRequestEnvelope struct {
	Stream          StreamIdentity
	Profile         RuntimeProfile
	ModelID         string
	Payload         string
	MaxTokens       uint32
	Temperature     float32
	StreamRequested bool
	HasOptions      bool
	Options         GenerationOptions
}

func ToLinePRequest(env Envelope) (req LinePRequestEnvelope, err error) {
	defer recoverError(&err)
	if env.TypeID() != "linep.v02.request" {
		fail(ErrType)
	}
	root := env.Value()
	payload := asObject(root.Object["payload"])

	streamObj := asObject(payload["stream"])
	req.Stream = StreamIdentity{
		RequestID:   ParseUint64(asString(streamObj["request_id"]), true),
		ExecutionID: ParseUint64(asString(streamObj["execution_id"]), true),
		OutputID:    uint32(asNumber(streamObj["output_id"])),
	}

	profStr := asString(payload["profile"])
	switch profStr {
	case "generate":
		req.Profile = ProfileGenerate
	case "chat":
		req.Profile = ProfileChat
	case "embed":
		req.Profile = ProfileEmbed
	default:
		fail(ErrSchema)
	}

	req.ModelID = asString(payload["model_id"])
	req.Payload = asString(payload["payload"])
	req.MaxTokens = uint32(asNumber(payload["max_tokens"]))
	req.Temperature = float32(asNumber(payload["temperature"]))
	req.StreamRequested = asBool(payload["stream_requested"])
	req.HasOptions = asBool(payload["has_options"])

	if req.HasOptions {
		optObj := asObject(payload["options"])
		req.Options = GenerationOptions{
			TopP:             float32(asNumber(optObj["top_p"])),
			TopK:             int32(asNumber(optObj["top_k"])),
			RepeatPenalty:    float32(asNumber(optObj["repeat_penalty"])),
			RepeatLastN:      int32(asNumber(optObj["repeat_last_n"])),
			Seed:             ParseUint64(asString(optObj["seed"]), false),
			PresencePenalty:  float32(asNumber(optObj["presence_penalty"])),
			FrequencyPenalty: float32(asNumber(optObj["frequency_penalty"])),
		}
		for _, s := range asArray(optObj["stop_sequences"]) {
			req.Options.StopSequences = append(req.Options.StopSequences, asString(s))
		}
		for _, pairVal := range asArray(optObj["extra_options"]) {
			pair := asArray(pairVal)
			req.Options.ExtraOptions = append(req.Options.ExtraOptions, [2]string{
				asString(pair[0]),
				asString(pair[1]),
			})
		}
	}
	return req, nil
}

func FromLinePRequest(req LinePRequestEnvelope, provenance map[string]Value, limits Limits) (env Envelope, err error) {
	defer recoverError(&err)
	var profStr string
	switch req.Profile {
	case ProfileGenerate:
		profStr = "generate"
	case ProfileChat:
		profStr = "chat"
	case ProfileEmbed:
		profStr = "embed"
	default:
		return Envelope{}, &Error{Code: ErrSchema}
	}

	streamObj := map[string]Value{
		"request_id":   NewString(strconv.FormatUint(req.Stream.RequestID, 10)),
		"execution_id": NewString(strconv.FormatUint(req.Stream.ExecutionID, 10)),
		"output_id":    NewNumber(float64(req.Stream.OutputID)),
	}

	payload := map[string]Value{
		"protocol_version": NewString("0.2"),
		"stream":           NewObject(streamObj),
		"profile":          NewString(profStr),
		"model_id":         NewString(req.ModelID),
		"payload":          NewString(req.Payload),
		"max_tokens":       NewNumber(float64(req.MaxTokens)),
		"temperature":      NewNumber(float64(req.Temperature)),
		"stream_requested": NewBool(req.StreamRequested),
		"has_options":      NewBool(req.HasOptions),
	}

	if req.HasOptions {
		stopSeqs := make([]Value, len(req.Options.StopSequences))
		for i, s := range req.Options.StopSequences {
			stopSeqs[i] = NewString(s)
		}

		sortedExtra := make([][2]string, len(req.Options.ExtraOptions))
		copy(sortedExtra, req.Options.ExtraOptions)
		sort.Slice(sortedExtra, func(i, j int) bool {
			return sortedExtra[i][0] < sortedExtra[j][0]
		})

		for i := 1; i < len(sortedExtra); i++ {
			if sortedExtra[i-1][0] == sortedExtra[i][0] {
				fail(ErrOptionKeys)
			}
		}

		extraOpts := make([]Value, len(sortedExtra))
		for i, pair := range sortedExtra {
			extraOpts[i] = NewArray([]Value{NewString(pair[0]), NewString(pair[1])})
		}

		optObj := map[string]Value{
			"top_p":             NewNumber(float64(req.Options.TopP)),
			"top_k":             NewNumber(float64(req.Options.TopK)),
			"repeat_penalty":    NewNumber(float64(req.Options.RepeatPenalty)),
			"repeat_last_n":     NewNumber(float64(req.Options.RepeatLastN)),
			"seed":              NewString(strconv.FormatUint(req.Options.Seed, 10)),
			"presence_penalty":  NewNumber(float64(req.Options.PresencePenalty)),
			"frequency_penalty": NewNumber(float64(req.Options.FrequencyPenalty)),
			"stop_sequences":    NewArray(stopSeqs),
			"extra_options":     NewArray(extraOpts),
		}
		payload["options"] = NewObject(optObj)
	}

	if provenance == nil {
		provenance = map[string]Value{
			"kind":      NewString("SOURCE_LITERAL"),
			"source_id": NewString("linep.adapter"),
		}
	}

	root := map[string]Value{
		"type_id":        NewString("linep.v02.request"),
		"schema_version": NewString("0.1"),
		"provenance":     NewObject(provenance),
		"payload":        NewObject(payload),
	}

	return CreatePilot(NewObject(root), "linep.v02.request", limits)
}
