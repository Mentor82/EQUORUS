package equorus

import "fmt"

type ErrorCode string

const (
	ErrMalformed    ErrorCode = "MALFORMED"
	ErrDuplicateKey ErrorCode = "DUPLICATE_KEY"
	ErrUnicode      ErrorCode = "UNICODE"
	ErrNumber       ErrorCode = "NUMBER"
	ErrLimit        ErrorCode = "LIMIT"
	ErrType         ErrorCode = "TYPE"
	ErrVersion      ErrorCode = "VERSION"
	ErrSchema       ErrorCode = "SCHEMA"
	ErrUint64Range  ErrorCode = "UINT64_RANGE"
	ErrFloat32      ErrorCode = "FLOAT32"
	ErrTimestamp    ErrorCode = "TIMESTAMP"
	ErrMetricUnit   ErrorCode = "METRIC_UNIT"
	ErrMetricValue  ErrorCode = "METRIC_VALUE"
	ErrOptionKeys   ErrorCode = "OPTION_KEYS"
	ErrProfile      ErrorCode = "PROFILE"
	ErrAlgorithm    ErrorCode = "ALGORITHM"
	ErrIntegrity    ErrorCode = "INTEGRITY"
	ErrInternal     ErrorCode = "INTERNAL"
)

type Error struct {
	Code ErrorCode
}

func (e *Error) Error() string {
	return string(e.Code)
}

func fail(code ErrorCode) {
	panic(&Error{Code: code})
}

func recoverError(err *error) {
	if r := recover(); r != nil {
		if e, ok := r.(*Error); ok {
			*err = e
		} else {
			*err = fmt.Errorf("panic: %v", r)
		}
	}
}
