package equorus

type Limits struct {
	MaxBytes        int
	MaxDepth        int
	MaxItems        int
	MaxStringLength int
}

const ImplementationMaxDepth = 128

func DefaultLimits() Limits {
	return Limits{
		MaxBytes:        65536,
		MaxDepth:        12,
		MaxItems:        2048,
		MaxStringLength: 8192,
	}
}

func (l Limits) Check() {
	if l.MaxBytes < 0 || l.MaxDepth < 0 || l.MaxItems < 0 || l.MaxStringLength < 0 || l.MaxDepth > ImplementationMaxDepth {
		fail(ErrLimit)
	}
}
