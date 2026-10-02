package equorus

import (
	"bytes"
	"crypto/sha256"
	"crypto/subtle"
	"encoding/binary"
	"encoding/hex"
	"math"
	"sort"
)

const (
	CanonicalProfile   = "equorus-value-v1"
	IntegrityAlgorithm = "sha-256"
)

var domainPrefix = []byte("EQUORUS-INTEGRITY\x00v1\x00")

type IntegrityRecord struct {
	Profile   string `json:"canonical_profile"`
	Algorithm string `json:"algorithm"`
	Digest    string `json:"digest"`
}

func (r IntegrityRecord) Validate() {
	if r.Profile != CanonicalProfile {
		fail(ErrProfile)
	}
	if r.Algorithm != IntegrityAlgorithm {
		fail(ErrAlgorithm)
	}
	if len(r.Digest) != 64 {
		fail(ErrIntegrity)
	}
	for i := 0; i < len(r.Digest); i++ {
		c := r.Digest[i]
		if !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) {
			fail(ErrIntegrity)
		}
	}
}

type canonicalWriter struct {
	limits Limits
	out    []byte
}

func (w *canonicalWriter) appendBytes(b []byte) {
	if len(w.out)+len(b) > w.limits.MaxBytes {
		fail(ErrLimit)
	}
	w.out = append(w.out, b...)
}

func (w *canonicalWriter) tag(c byte) {
	w.appendBytes([]byte{c})
}

func (w *canonicalWriter) u64(n uint64) {
	var b [8]byte
	binary.BigEndian.PutUint64(b[:], n)
	w.appendBytes(b[:])
}

func (w *canonicalWriter) string(s string) {
	w.tag('s')
	w.u64(uint64(len(s)))
	w.appendBytes([]byte(s))
}

func (w *canonicalWriter) value(v Value) {
	switch v.Kind {
	case KindNull:
		w.tag('n')
	case KindBool:
		if v.Bool {
			w.tag('t')
		} else {
			w.tag('f')
		}
	case KindNumber:
		w.tag('d')
		w.u64(math.Float64bits(v.Number))
	case KindString:
		w.string(v.String)
	case KindArray:
		w.tag('a')
		w.u64(uint64(len(v.Array)))
		for _, item := range v.Array {
			w.value(item)
		}
	case KindObject:
		w.tag('o')
		w.u64(uint64(len(v.Object)))
		keys := make([]string, 0, len(v.Object))
		for k := range v.Object {
			keys = append(keys, k)
		}
		sort.Slice(keys, func(i, j int) bool {
			return bytes.Compare([]byte(keys[i]), []byte(keys[j])) < 0
		})
		for _, k := range keys {
			w.string(k)
			w.value(v.Object[k])
		}
	default:
		fail(ErrSchema)
	}
}

func CanonicalBytes(v Value, profile string, limits Limits) (res []byte, err error) {
	defer recoverError(&err)
	if profile != CanonicalProfile {
		fail(ErrProfile)
	}
	ValidateValue(v, limits)
	w := &canonicalWriter{
		limits: limits,
		out:    make([]byte, 0, 128),
	}
	w.value(v)
	return w.out, nil
}

func ComputeIntegrity(envelope Envelope, profile, algorithm string, limits Limits) (rec IntegrityRecord, err error) {
	defer recoverError(&err)
	if profile != CanonicalProfile {
		fail(ErrProfile)
	}
	if algorithm != IntegrityAlgorithm {
		fail(ErrAlgorithm)
	}
	canon, err := CanonicalBytes(envelope.Value(), profile, limits)
	if err != nil {
		return IntegrityRecord{}, err
	}
	h := sha256.New()
	h.Write(domainPrefix)
	h.Write([]byte(profile))
	h.Write([]byte{0})
	h.Write([]byte(algorithm))
	h.Write([]byte{0})
	h.Write(canon)
	digest := hex.EncodeToString(h.Sum(nil))
	return IntegrityRecord{
		Profile:   profile,
		Algorithm: algorithm,
		Digest:    digest,
	}, nil
}

func VerifyIntegrity(envelope Envelope, record IntegrityRecord, limits Limits) (ok bool, err error) {
	defer recoverError(&err)
	record.Validate()
	actual, err := ComputeIntegrity(envelope, record.Profile, record.Algorithm, limits)
	if err != nil {
		return false, err
	}
	return subtle.ConstantTimeCompare([]byte(actual.Digest), []byte(record.Digest)) == 1, nil
}

func EncodeIntegrity(record IntegrityRecord, limits Limits) (res []byte, err error) {
	defer recoverError(&err)
	record.Validate()
	obj := map[string]Value{
		"canonical_profile": NewString(record.Profile),
		"algorithm":         NewString(record.Algorithm),
		"digest":            NewString(record.Digest),
	}
	return (JsonCodec{}).Encode(NewObject(obj), limits)
}

func DecodeIntegrity(raw []byte, limits Limits) (rec IntegrityRecord, err error) {
	defer recoverError(&err)
	val, err := (JsonCodec{}).Decode(raw, limits)
	if err != nil {
		return IntegrityRecord{}, err
	}
	if val.Kind != KindObject || len(val.Object) != 3 {
		fail(ErrIntegrity)
	}
	cp, ok1 := val.Object["canonical_profile"]
	alg, ok2 := val.Object["algorithm"]
	dig, ok3 := val.Object["digest"]
	if !ok1 || !ok2 || !ok3 || cp.Kind != KindString || alg.Kind != KindString || dig.Kind != KindString {
		fail(ErrIntegrity)
	}
	record := IntegrityRecord{
		Profile:   cp.String,
		Algorithm: alg.String,
		Digest:    dig.String,
	}
	record.Validate()
	return record, nil
}
