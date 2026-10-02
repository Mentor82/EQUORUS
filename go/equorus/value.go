package equorus

import (
	"math"
	"strconv"
)

type ValueKind int

const (
	KindNull ValueKind = iota
	KindBool
	KindNumber
	KindString
	KindArray
	KindObject
)

type Value struct {
	Kind   ValueKind
	Bool   bool
	Number float64
	String string
	Array  []Value
	Object map[string]Value
}

func NewNull() Value {
	return Value{Kind: KindNull}
}

func NewBool(b bool) Value {
	return Value{Kind: KindBool, Bool: b}
}

func NewNumber(n float64) Value {
	return Value{Kind: KindNumber, Number: n}
}

func NewString(s string) Value {
	return Value{Kind: KindString, String: s}
}

func NewArray(items []Value) Value {
	if items == nil {
		items = []Value{}
	}
	return Value{Kind: KindArray, Array: items}
}

func NewObject(members map[string]Value) Value {
	if members == nil {
		members = make(map[string]Value)
	}
	return Value{Kind: KindObject, Object: members}
}

func (v Value) Equal(other Value) bool {
	if v.Kind != other.Kind {
		return false
	}
	switch v.Kind {
	case KindNull:
		return true
	case KindBool:
		return v.Bool == other.Bool
	case KindNumber:
		if math.IsNaN(v.Number) || math.IsNaN(other.Number) {
			return false
		}
		if v.Number == 0 && other.Number == 0 {
			return math.Signbit(v.Number) == math.Signbit(other.Number)
		}
		return v.Number == other.Number
	case KindString:
		return v.String == other.String
	case KindArray:
		if len(v.Array) != len(other.Array) {
			return false
		}
		for i := range v.Array {
			if !v.Array[i].Equal(other.Array[i]) {
				return false
			}
		}
		return true
	case KindObject:
		if len(v.Object) != len(other.Object) {
			return false
		}
		for k, val := range v.Object {
			oval, ok := other.Object[k]
			if !ok || !val.Equal(oval) {
				return false
			}
		}
		return true
	default:
		return false
	}
}

func (v Value) Clone() Value {
	switch v.Kind {
	case KindArray:
		arr := make([]Value, len(v.Array))
		for i, item := range v.Array {
			arr[i] = item.Clone()
		}
		return Value{Kind: KindArray, Array: arr}
	case KindObject:
		obj := make(map[string]Value, len(v.Object))
		for k, val := range v.Object {
			obj[k] = val.Clone()
		}
		return Value{Kind: KindObject, Object: obj}
	default:
		return v
	}
}

func ValidateNumber(v float64) {
	if math.IsNaN(v) || math.IsInf(v, 0) || (v == 0 && math.Signbit(v)) {
		fail(ErrNumber)
	}
	if math.Trunc(v) == v && math.Abs(v) > 9007199254740991.0 {
		fail(ErrNumber)
	}
}

func UnicodeLength(s string) int {
	count := 0
	i := 0
	nBytes := len(s)
	for i < nBytes {
		c := s[i]
		i++
		count++
		if c < 0x80 {
			continue
		}
		var n int
		var cp uint32
		var minimum uint32
		if c >= 0xC2 && c <= 0xDF {
			n = 1
			cp = uint32(c & 31)
			minimum = 0x80
		} else if c >= 0xE0 && c <= 0xEF {
			n = 2
			cp = uint32(c & 15)
			minimum = 0x800
		} else if c >= 0xF0 && c <= 0xF4 {
			n = 3
			cp = uint32(c & 7)
			minimum = 0x10000
		} else {
			fail(ErrUnicode)
		}

		if n > nBytes-i {
			fail(ErrUnicode)
		}
		for k := 0; k < n; k++ {
			b := s[i]
			i++
			if (b & 0xC0) != 0x80 {
				fail(ErrUnicode)
			}
			cp = (cp << 6) | uint32(b&63)
		}
		if cp < minimum || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF) {
			fail(ErrUnicode)
		}
	}
	return count
}

func ParseUint64(s string, nonzero bool) uint64 {
	if len(s) == 0 || (len(s) > 1 && s[0] == '0') || s[0] == '-' || s[0] == '+' {
		fail(ErrSchema)
	}
	for i := 0; i < len(s); i++ {
		if s[i] < '0' || s[i] > '9' {
			fail(ErrSchema)
		}
	}
	v, err := strconv.ParseUint(s, 10, 64)
	if err != nil {
		fail(ErrUint64Range)
	}
	if nonzero && v == 0 {
		fail(ErrUint64Range)
	}
	return v
}

func ValidateValue(v Value, limits Limits) {
	limits.Check()
	count := 0
	var walk func(val Value, depth int)
	walk = func(val Value, depth int) {
		if depth > limits.MaxDepth || count >= limits.MaxItems {
			fail(ErrLimit)
		}
		count++
		switch val.Kind {
		case KindNull, KindBool:
			// ok
		case KindNumber:
			ValidateNumber(val.Number)
		case KindString:
			if len(val.String) > limits.MaxStringLength {
				fail(ErrLimit)
			}
			UnicodeLength(val.String)
		case KindArray:
			for _, item := range val.Array {
				walk(item, depth+1)
			}
		case KindObject:
			for k, item := range val.Object {
				if len(k) > limits.MaxStringLength {
					fail(ErrLimit)
				}
				UnicodeLength(k)
				walk(item, depth+1)
			}
		default:
			fail(ErrSchema)
		}
	}
	walk(v, 1)
}
