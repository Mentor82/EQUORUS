package equorus

import (
	"bytes"
	"fmt"
	"sort"
	"strconv"
)

type jsonWriter struct {
	limits Limits
	out    []byte
}

func (w *jsonWriter) appendBytes(b []byte) {
	if len(w.out)+len(b) > w.limits.MaxBytes {
		fail(ErrLimit)
	}
	w.out = append(w.out, b...)
}

func (w *jsonWriter) appendString(s string) {
	w.appendBytes([]byte{'"'})
	for i := 0; i < len(s); i++ {
		c := s[i]
		if c == '"' {
			w.appendBytes([]byte{'\\', '"'})
		} else if c == '\\' {
			w.appendBytes([]byte{'\\', '\\'})
		} else if c < 0x20 {
			w.appendBytes([]byte(fmt.Sprintf("\\u%04x", c)))
		} else {
			w.appendBytes([]byte{c})
		}
	}
	w.appendBytes([]byte{'"'})
}

func (w *jsonWriter) appendValue(v Value) {
	switch v.Kind {
	case KindNull:
		w.appendBytes([]byte("null"))
	case KindBool:
		if v.Bool {
			w.appendBytes([]byte("true"))
		} else {
			w.appendBytes([]byte("false"))
		}
	case KindNumber:
		ValidateNumber(v.Number)
		str := strconv.FormatFloat(v.Number, 'g', -1, 64)
		w.appendBytes([]byte(str))
	case KindString:
		w.appendString(v.String)
	case KindArray:
		w.appendBytes([]byte{'['})
		for i, item := range v.Array {
			if i > 0 {
				w.appendBytes([]byte{','})
			}
			w.appendValue(item)
		}
		w.appendBytes([]byte{']'})
	case KindObject:
		w.appendBytes([]byte{'{'})
		keys := make([]string, 0, len(v.Object))
		for k := range v.Object {
			keys = append(keys, k)
		}
		// Sort keys lexicographically by unsigned UTF-8 bytes
		sort.Slice(keys, func(i, j int) bool {
			return bytes.Compare([]byte(keys[i]), []byte(keys[j])) < 0
		})
		for i, k := range keys {
			if i > 0 {
				w.appendBytes([]byte{','})
			}
			w.appendString(k)
			w.appendBytes([]byte{':'})
			w.appendValue(v.Object[k])
		}
		w.appendBytes([]byte{'}'})
	default:
		fail(ErrSchema)
	}
}

func encodeJSON(v Value, limits Limits) []byte {
	ValidateValue(v, limits)
	w := &jsonWriter{
		limits: limits,
		out:    make([]byte, 0, 128),
	}
	w.appendValue(v)
	return w.out
}
