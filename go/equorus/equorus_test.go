package equorus

import (
	"bytes"
	"encoding/hex"
	"encoding/json"
	"os"
	"path/filepath"
	"testing"
)

type vectorValue struct {
	Name string `json:"name"`
	JSON string `json:"json"`
	Hex  string `json:"hex"`
}

type vectorEnvelope struct {
	File   string `json:"file"`
	Hex    string `json:"hex"`
	Digest string `json:"digest"`
}

type vectorsFile struct {
	Profile   string           `json:"profile"`
	Algorithm string           `json:"algorithm"`
	Values    []vectorValue    `json:"values"`
	Envelopes []vectorEnvelope `json:"envelopes"`
}

type mutationChange struct {
	Path  []interface{} `json:"path"`
	Op    string        `json:"op"`
	Value interface{}   `json:"value"`
}

type caseLimits struct {
	MaxBytes        *int `json:"max_bytes"`
	MaxDepth        *int `json:"max_depth"`
	MaxItems        *int `json:"max_items"`
	MaxStringLength *int `json:"max_string_length"`
}

type testCase struct {
	Name         string           `json:"name"`
	Base         string           `json:"base"`
	ExpectedType string           `json:"expected_type"`
	Expect       string           `json:"expect"`
	Raw          *string          `json:"raw"`
	RawHex       *string          `json:"raw_hex"`
	Limits       *caseLimits      `json:"limits"`
	Changes      []mutationChange `json:"changes"`
}

func TestCanonicalVectors(t *testing.T) {
	vecPath := filepath.Join("..", "..", "tests", "fixtures", "canonical-v1", "vectors.json")
	data, err := os.ReadFile(vecPath)
	if err != nil {
		t.Fatalf("failed to read vectors.json: %v", err)
	}

	var vec vectorsFile
	if err := json.Unmarshal(data, &vec); err != nil {
		t.Fatalf("failed to unmarshal vectors: %v", err)
	}

	if vec.Profile != CanonicalProfile {
		t.Fatalf("unexpected profile: %s", vec.Profile)
	}

	codec := JsonCodec{}
	limits := DefaultLimits()

	for _, v := range vec.Values {
		t.Run("Value/"+v.Name, func(t *testing.T) {
			val, err := codec.Decode([]byte(v.JSON), limits)
			if err != nil {
				t.Fatalf("decode failed: %v", err)
			}
			expected, err := hex.DecodeString(v.Hex)
			if err != nil {
				t.Fatalf("hex decode failed: %v", err)
			}
			actual, err := CanonicalBytes(val, vec.Profile, limits)
			if err != nil {
				t.Fatalf("canonical_bytes failed: %v", err)
			}
			if !hexEqual(actual, expected) {
				t.Fatalf("bytes mismatch for %s: got %x, want %x", v.Name, actual, expected)
			}
		})
	}

	for _, envVec := range vec.Envelopes {
		t.Run("Envelope/"+envVec.File, func(t *testing.T) {
			raw, err := os.ReadFile(filepath.Join("..", "..", "tests", "fixtures", "pilot-v0.1", envVec.File))
			if err != nil {
				t.Fatalf("failed to read fixture %s: %v", envVec.File, err)
			}
			val, err := codec.Decode(raw, limits)
			if err != nil {
				t.Fatalf("decode failed: %v", err)
			}
			typeID := asString(val.Object["type_id"])
			env, err := CreatePilot(val, typeID, limits)
			if err != nil {
				t.Fatalf("create pilot failed: %v", err)
			}

			expectedBytes, err := hex.DecodeString(envVec.Hex)
			if err != nil {
				t.Fatalf("hex decode failed: %v", err)
			}

			actualBytes, err := CanonicalBytes(env.Value(), vec.Profile, limits)
			if err != nil {
				t.Fatalf("canonical bytes failed: %v", err)
			}
			if !hexEqual(actualBytes, expectedBytes) {
				t.Fatalf("canonical bytes mismatch: got %x, want %x", actualBytes, expectedBytes)
			}

			rec, err := ComputeIntegrity(env, vec.Profile, IntegrityAlgorithm, limits)
			if err != nil {
				t.Fatalf("compute integrity failed: %v", err)
			}
			if rec.Digest != envVec.Digest {
				t.Fatalf("digest mismatch: got %s, want %s", rec.Digest, envVec.Digest)
			}

			ok, err := VerifyIntegrity(env, rec, limits)
			if err != nil || !ok {
				t.Fatalf("verify failed: ok=%v, err=%v", ok, err)
			}
		})
	}
}

func hexEqual(a, b []byte) bool {
	if len(a) != len(b) {
		return false
	}
	for i := range a {
		if a[i] != b[i] {
			return false
		}
	}
	return true
}

func TestPilotCases(t *testing.T) {
	casesPath := filepath.Join("..", "..", "tests", "fixtures", "pilot-v0.1", "cases.json")
	data, err := os.ReadFile(casesPath)
	if err != nil {
		t.Fatalf("failed to read cases.json: %v", err)
	}

	var cases []testCase
	if err := json.Unmarshal(data, &cases); err != nil {
		t.Fatalf("failed to unmarshal cases: %v", err)
	}

	fixturesDir := filepath.Join("..", "..", "tests", "fixtures", "pilot-v0.1")

	for _, c := range cases {
		t.Run(c.Name, func(t *testing.T) {
			expectedType := c.ExpectedType
			if expectedType == "" {
				if c.Base != "" {
					baseBytes, err := os.ReadFile(filepath.Join(fixturesDir, c.Base))
					if err != nil {
						t.Fatalf("failed to read base %s: %v", c.Base, err)
					}
					var baseMap map[string]interface{}
					if err := json.Unmarshal(baseBytes, &baseMap); err != nil {
						t.Fatalf("unmarshal base failed: %v", err)
					}
					expectedType = baseMap["type_id"].(string)
				} else {
					expectedType = "vinox.provenance.snapshot"
				}
			}

			limits := DefaultLimits()
			if c.Limits != nil {
				if c.Limits.MaxBytes != nil {
					limits.MaxBytes = *c.Limits.MaxBytes
				}
				if c.Limits.MaxDepth != nil {
					limits.MaxDepth = *c.Limits.MaxDepth
				}
				if c.Limits.MaxItems != nil {
					limits.MaxItems = *c.Limits.MaxItems
				}
				if c.Limits.MaxStringLength != nil {
					limits.MaxStringLength = *c.Limits.MaxStringLength
				}
			}

			var raw []byte
			if c.Raw != nil {
				raw = []byte(*c.Raw)
			} else if c.RawHex != nil {
				b, err := hex.DecodeString(*c.RawHex)
				if err != nil {
					t.Fatalf("invalid hex in test case: %v", err)
				}
				raw = b
			} else {
				baseBytes, err := os.ReadFile(filepath.Join(fixturesDir, c.Base))
				if err != nil {
					t.Fatalf("failed to read base %s: %v", c.Base, err)
				}
				if c.Name == "lone surrogate" {
					raw = bytes.Replace(baseBytes, []byte(`"source_id": "fixture:vinox/tool/α"`), []byte(`"source_id": "\ud800"`), 1)
				} else if len(c.Changes) > 0 {
					var obj interface{}
					if err := json.Unmarshal(baseBytes, &obj); err != nil {
						t.Fatalf("unmarshal base failed: %v", err)
					}
					for _, ch := range c.Changes {
						applyMutation(&obj, ch.Path, ch.Op, ch.Value)
					}
					mutated, err := json.Marshal(obj)
					if err != nil {
						t.Fatalf("marshal mutated failed: %v", err)
					}
					raw = mutated
				} else {
					raw = baseBytes
				}
			}

			_, err := DecodePilot(raw, expectedType, limits)
			if c.Expect == "accept" {
				if err != nil {
					t.Fatalf("expected accept, got error: %v", err)
				}
			} else {
				if err == nil {
					t.Fatalf("expected error %s, got success", c.Expect)
				}
				eqErr, ok := err.(*Error)
				if !ok {
					t.Fatalf("expected *Error, got %T: %v", err, err)
				}
				if string(eqErr.Code) != c.Expect {
					t.Fatalf("expected error code %s, got %s", c.Expect, eqErr.Code)
				}
			}
		})
	}
}

func applyMutation(root *interface{}, path []interface{}, op string, val interface{}) {
	if len(path) == 0 {
		return
	}
	curr := *root
	for i := 0; i < len(path)-1; i++ {
		key := path[i]
		switch k := key.(type) {
		case string:
			m := curr.(map[string]interface{})
			curr = m[k]
		case float64:
			idx := int(k)
			arr := curr.([]interface{})
			curr = arr[idx]
		}
	}
	lastKey := path[len(path)-1]
	switch k := lastKey.(type) {
	case string:
		m := curr.(map[string]interface{})
		if op == "remove" {
			delete(m, k)
		} else {
			m[k] = val
		}
	case float64:
		idx := int(k)
		arr := curr.([]interface{})
		if op == "remove" {
			arr = append(arr[:idx], arr[idx+1:]...)
		} else {
			arr[idx] = val
		}
	}
}

func TestLinePAdapterRoundtrip(t *testing.T) {
	limits := DefaultLimits()
	for _, fixture := range []string{"linep-options.json", "linep-no-options.json"} {
		t.Run(fixture, func(t *testing.T) {
			raw, err := os.ReadFile(filepath.Join("..", "..", "tests", "fixtures", "pilot-v0.1", fixture))
			if err != nil {
				t.Fatalf("failed to read fixture: %v", err)
			}
			env, err := DecodePilot(raw, "linep.v02.request", limits)
			if err != nil {
				t.Fatalf("failed to decode pilot: %v", err)
			}

			req, err := ToLinePRequest(env)
			if err != nil {
				t.Fatalf("ToLinePRequest failed: %v", err)
			}

			env2, err := FromLinePRequest(req, env.Value().Object["provenance"].Object, limits)
			if err != nil {
				t.Fatalf("FromLinePRequest failed: %v", err)
			}

			c1, err := CanonicalBytes(env.Value(), CanonicalProfile, limits)
			if err != nil {
				t.Fatalf("canonical 1 failed: %v", err)
			}
			c2, err := CanonicalBytes(env2.Value(), CanonicalProfile, limits)
			if err != nil {
				t.Fatalf("canonical 2 failed: %v", err)
			}
			if !hexEqual(c1, c2) {
				t.Fatalf("roundtrip canonical bytes mismatch for %s", fixture)
			}
		})
	}
}

func TestLinePAdapterExtraOptions(t *testing.T) {
	limits := DefaultLimits()
	req := LinePRequestEnvelope{
		Stream: StreamIdentity{
			RequestID:   1,
			ExecutionID: 2,
			OutputID:    0,
		},
		Profile:         ProfileChat,
		ModelID:         "test",
		Payload:         "hello",
		MaxTokens:       16,
		Temperature:     0.5,
		StreamRequested: false,
		HasOptions:      true,
		Options: GenerationOptions{
			TopP:             0.9,
			TopK:             40,
			RepeatPenalty:    1.0,
			RepeatLastN:      64,
			Seed:             42,
			PresencePenalty:  0.0,
			FrequencyPenalty: 0.0,
			ExtraOptions:     [][2]string{{"z", "2"}, {"a", "1"}},
		},
	}

	env, err := FromLinePRequest(req, nil, limits)
	if err != nil {
		t.Fatalf("FromLinePRequest failed: %v", err)
	}

	back, err := ToLinePRequest(env)
	if err != nil {
		t.Fatalf("ToLinePRequest failed: %v", err)
	}

	if len(back.Options.ExtraOptions) != 2 ||
		back.Options.ExtraOptions[0] != [2]string{"a", "1"} ||
		back.Options.ExtraOptions[1] != [2]string{"z", "2"} {
		t.Fatalf("expected sorted extra_options, got: %v", back.Options.ExtraOptions)
	}

	req.Options.ExtraOptions = [][2]string{{"k", "1"}, {"k", "2"}}
	if _, err := FromLinePRequest(req, nil, limits); err == nil {
		t.Fatalf("expected error on duplicate extra_options keys")
	}
}
