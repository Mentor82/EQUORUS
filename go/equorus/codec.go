package equorus

type Codec interface {
	Decode(raw []byte, limits Limits) (Value, error)
	Encode(v Value, limits Limits) ([]byte, error)
}

type JsonCodec struct{}

func (c JsonCodec) Decode(raw []byte, limits Limits) (val Value, err error) {
	defer recoverError(&err)
	val = parseJSON(raw, limits)
	return val, nil
}

func (c JsonCodec) Encode(v Value, limits Limits) (res []byte, err error) {
	defer recoverError(&err)
	res = encodeJSON(v, limits)
	return res, nil
}
