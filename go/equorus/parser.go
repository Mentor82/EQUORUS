package equorus

import (
	"strconv"
	"strings"
	"unicode/utf8"
)

type parser struct {
	data   []byte
	pos    int
	limits Limits
	items  int
}

func parseJSON(raw []byte, limits Limits) (res Value) {
	limits.Check()
	if len(raw) > limits.MaxBytes {
		fail(ErrLimit)
	}
	if !utf8.Valid(raw) {
		fail(ErrMalformed)
	}
	p := &parser{
		data:   raw,
		pos:    0,
		limits: limits,
		items:  0,
	}
	val := p.parseValue(1)
	p.skipWS()
	if p.pos != len(p.data) {
		fail(ErrMalformed)
	}
	return val
}

func (p *parser) skipWS() {
	for p.pos < len(p.data) {
		c := p.data[p.pos]
		if c == ' ' || c == '\t' || c == '\r' || c == '\n' {
			p.pos++
		} else {
			break
		}
	}
}

func (p *parser) take() byte {
	if p.pos >= len(p.data) {
		fail(ErrMalformed)
	}
	c := p.data[p.pos]
	p.pos++
	return c
}

func (p *parser) hex4() uint32 {
	if p.pos+4 > len(p.data) {
		fail(ErrMalformed)
	}
	var val uint32
	for i := 0; i < 4; i++ {
		c := p.data[p.pos+i]
		val <<= 4
		if c >= '0' && c <= '9' {
			val |= uint32(c - '0')
		} else if c >= 'a' && c <= 'f' {
			val |= uint32(c - 'a' + 10)
		} else if c >= 'A' && c <= 'F' {
			val |= uint32(c - 'A' + 10)
		} else {
			fail(ErrMalformed)
		}
	}
	p.pos += 4
	return val
}

func (p *parser) parseString() string {
	if p.take() != '"' {
		fail(ErrMalformed)
	}
	var sb strings.Builder
	size := 0
	buf := make([]byte, 4)

	for {
		if p.pos >= len(p.data) {
			fail(ErrMalformed)
		}
		c := p.take()
		if c == '"' {
			return sb.String()
		}
		if c < 0x20 {
			fail(ErrMalformed)
		}
		if c == '\\' {
			esc := p.take()
			switch esc {
			case '"', '\\', '/':
				size++
				if size > p.limits.MaxStringLength {
					fail(ErrLimit)
				}
				sb.WriteByte(esc)
			case 'b':
				size++
				if size > p.limits.MaxStringLength {
					fail(ErrLimit)
				}
				sb.WriteByte('\b')
			case 'f':
				size++
				if size > p.limits.MaxStringLength {
					fail(ErrLimit)
				}
				sb.WriteByte('\f')
			case 'n':
				size++
				if size > p.limits.MaxStringLength {
					fail(ErrLimit)
				}
				sb.WriteByte('\n')
			case 'r':
				size++
				if size > p.limits.MaxStringLength {
					fail(ErrLimit)
				}
				sb.WriteByte('\r')
			case 't':
				size++
				if size > p.limits.MaxStringLength {
					fail(ErrLimit)
				}
				sb.WriteByte('\t')
			case 'u':
				cp := p.hex4()
				if cp >= 0xD800 && cp <= 0xDBFF {
					if p.pos+2 > len(p.data) || p.take() != '\\' || p.take() != 'u' {
						fail(ErrUnicode)
					}
					low := p.hex4()
					if low < 0xDC00 || low > 0xDFFF {
						fail(ErrUnicode)
					}
					cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00)
				} else if cp >= 0xDC00 && cp <= 0xDFFF {
					fail(ErrUnicode)
				}
				n := utf8.EncodeRune(buf, rune(cp))
				size += n
				if size > p.limits.MaxStringLength {
					fail(ErrLimit)
				}
				sb.Write(buf[:n])
			default:
				fail(ErrMalformed)
			}
		} else {
			// utf-8 multibyte or ascii
			p.pos--
			r, n := utf8.DecodeRune(p.data[p.pos:])
			if r == utf8.RuneError && n == 1 {
				fail(ErrMalformed)
			}
			p.pos += n
			size += n
			if size > p.limits.MaxStringLength {
				fail(ErrLimit)
			}
			sb.Write(p.data[p.pos-n : p.pos])
		}
	}
}

func (p *parser) parseNumber() float64 {
	start := p.pos
	if p.pos < len(p.data) && p.data[p.pos] == '-' {
		p.pos++
	}
	if p.pos >= len(p.data) || p.data[p.pos] < '0' || p.data[p.pos] > '9' {
		fail(ErrMalformed)
	}
	if p.data[p.pos] == '0' {
		p.pos++
	} else {
		for p.pos < len(p.data) && p.data[p.pos] >= '0' && p.data[p.pos] <= '9' {
			p.pos++
		}
	}
	if p.pos < len(p.data) && p.data[p.pos] == '.' {
		p.pos++
		if p.pos >= len(p.data) || p.data[p.pos] < '0' || p.data[p.pos] > '9' {
			fail(ErrMalformed)
		}
		for p.pos < len(p.data) && p.data[p.pos] >= '0' && p.data[p.pos] <= '9' {
			p.pos++
		}
	}
	if p.pos < len(p.data) && (p.data[p.pos] == 'e' || p.data[p.pos] == 'E') {
		p.pos++
		if p.pos < len(p.data) && (p.data[p.pos] == '+' || p.data[p.pos] == '-') {
			p.pos++
		}
		if p.pos >= len(p.data) || p.data[p.pos] < '0' || p.data[p.pos] > '9' {
			fail(ErrMalformed)
		}
		for p.pos < len(p.data) && p.data[p.pos] >= '0' && p.data[p.pos] <= '9' {
			p.pos++
		}
	}
	token := string(p.data[start:p.pos])
	val, err := strconv.ParseFloat(token, 64)
	if err != nil {
		fail(ErrNumber)
	}
	ValidateNumber(val)
	if val == 0.0 {
		mantissa := strings.Split(strings.ToLower(token), "e")[0]
		for i := 0; i < len(mantissa); i++ {
			if mantissa[i] >= '1' && mantissa[i] <= '9' {
				fail(ErrNumber)
			}
		}
	}
	return val
}

func (p *parser) parseValue(depth int) Value {
	p.skipWS()
	if p.pos >= len(p.data) {
		fail(ErrMalformed)
	}
	if depth > p.limits.MaxDepth || p.items >= p.limits.MaxItems {
		fail(ErrLimit)
	}
	p.items++

	c := p.data[p.pos]
	if c == '{' {
		p.pos++
		obj := make(map[string]Value)
		p.skipWS()
		if p.pos < len(p.data) && p.data[p.pos] == '}' {
			p.pos++
			return NewObject(obj)
		}
		for {
			p.skipWS()
			if p.pos >= len(p.data) || p.data[p.pos] != '"' {
				fail(ErrMalformed)
			}
			key := p.parseString()
			if _, exists := obj[key]; exists {
				fail(ErrDuplicateKey)
			}
			p.skipWS()
			if p.pos >= len(p.data) || p.data[p.pos] != ':' {
				fail(ErrMalformed)
			}
			p.pos++ // consume ':'
			val := p.parseValue(depth + 1)
			obj[key] = val
			p.skipWS()
			if p.pos >= len(p.data) {
				fail(ErrMalformed)
			}
			if p.data[p.pos] == '}' {
				p.pos++
				return NewObject(obj)
			}
			if p.data[p.pos] != ',' {
				fail(ErrMalformed)
			}
			p.pos++ // consume ','
		}
	}

	if c == '[' {
		p.pos++
		arr := make([]Value, 0)
		p.skipWS()
		if p.pos < len(p.data) && p.data[p.pos] == ']' {
			p.pos++
			return NewArray(arr)
		}
		for {
			val := p.parseValue(depth + 1)
			arr = append(arr, val)
			p.skipWS()
			if p.pos >= len(p.data) {
				fail(ErrMalformed)
			}
			if p.data[p.pos] == ']' {
				p.pos++
				return NewArray(arr)
			}
			if p.data[p.pos] != ',' {
				fail(ErrMalformed)
			}
			p.pos++ // consume ','
		}
	}

	if c == '"' {
		return NewString(p.parseString())
	}

	remaining := p.data[p.pos:]
	if strings.HasPrefix(string(remaining), "NaN") ||
		strings.HasPrefix(string(remaining), "Infinity") ||
		strings.HasPrefix(string(remaining), "-Infinity") {
		fail(ErrNumber)
	}

	if strings.HasPrefix(string(remaining), "true") {
		p.pos += 4
		return NewBool(true)
	}
	if strings.HasPrefix(string(remaining), "false") {
		p.pos += 5
		return NewBool(false)
	}
	if strings.HasPrefix(string(remaining), "null") {
		p.pos += 4
		return NewNull()
	}

	if c == '-' || (c >= '0' && c <= '9') {
		return NewNumber(p.parseNumber())
	}

	fail(ErrMalformed)
	return Value{}
}
