package equorus

type Validator func(v Value)

type Envelope struct {
	root Value
}

func field(root Value, name string, code ErrorCode) string {
	if root.Kind != KindObject {
		fail(ErrSchema)
	}
	v, ok := root.Object[name]
	if !ok || v.Kind != KindString {
		fail(code)
	}
	return v.String
}

func CreateEnvelope(root Value, expected string, versions []string, validator Validator, limits Limits) (env Envelope, err error) {
	defer recoverError(&err)
	ValidateValue(root, limits)
	if expected == "" || field(root, "type_id", ErrType) != expected {
		fail(ErrType)
	}
	version := field(root, "schema_version", ErrVersion)
	found := false
	for _, v := range versions {
		if v == version {
			found = true
			break
		}
	}
	if !found {
		fail(ErrVersion)
	}
	if len(root.Object) != 4 {
		fail(ErrSchema)
	}
	prov, ok := root.Object["provenance"]
	if !ok || prov.Kind != KindObject {
		fail(ErrSchema)
	}
	if _, ok := root.Object["payload"]; !ok {
		fail(ErrSchema)
	}
	if validator == nil {
		fail(ErrSchema)
	}
	validator(root)
	return Envelope{root: root.Clone()}, nil
}

func DecodeEnvelope(codec Codec, bytes []byte, expected string, versions []string, validator Validator, limits Limits) (Envelope, error) {
	val, err := codec.Decode(bytes, limits)
	if err != nil {
		return Envelope{}, err
	}
	return CreateEnvelope(val, expected, versions, validator, limits)
}

func (e Envelope) Value() Value {
	return e.root.Clone()
}

func (e Envelope) Encode(codec Codec, limits Limits) ([]byte, error) {
	return codec.Encode(e.root, limits)
}

func (e Envelope) TypeID() string {
	return field(e.root, "type_id", ErrType)
}

func (e Envelope) SchemaVersion() string {
	return field(e.root, "schema_version", ErrVersion)
}
