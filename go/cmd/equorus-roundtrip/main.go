package main

import (
	"fmt"
	"io"
	"os"
	"strconv"

	"github.com/Mentor82/EQUORUS/go/equorus"
)

func main() {
	if len(os.Args) != 2 && len(os.Args) != 6 {
		os.Exit(2)
	}

	limits := equorus.DefaultLimits()
	if len(os.Args) == 6 {
		mb, err1 := strconv.Atoi(os.Args[2])
		md, err2 := strconv.Atoi(os.Args[3])
		mi, err3 := strconv.Atoi(os.Args[4])
		ms, err4 := strconv.Atoi(os.Args[5])
		if err1 != nil || err2 != nil || err3 != nil || err4 != nil {
			os.Exit(2)
		}
		limits.MaxBytes = mb
		limits.MaxDepth = md
		limits.MaxItems = mi
		limits.MaxStringLength = ms
	}

	expectedType := os.Args[1]

	raw, err := io.ReadAll(os.Stdin)
	if err != nil {
		fmt.Fprintf(os.Stderr, "read error: %v\n", err)
		os.Exit(2)
	}
	if len(raw) > limits.MaxBytes {
		fmt.Printf("ERROR %s", equorus.ErrLimit)
		os.Exit(1)
	}

	env, err := equorus.DecodePilot(raw, expectedType, limits)
	if err != nil {
		if e, ok := err.(*equorus.Error); ok {
			fmt.Printf("ERROR %s", e.Code)
			os.Exit(1)
		}
		fmt.Fprintf(os.Stderr, "internal error: %v\n", err)
		os.Exit(2)
	}

	snapshot, err := equorus.CreatePilot(env.Value(), expectedType, limits)
	if err != nil {
		if e, ok := err.(*equorus.Error); ok {
			fmt.Printf("ERROR %s", e.Code)
			os.Exit(1)
		}
		fmt.Fprintf(os.Stderr, "internal error: %v\n", err)
		os.Exit(2)
	}

	encoded, err := snapshot.Encode(equorus.JsonCodec{}, limits)
	if err != nil {
		if e, ok := err.(*equorus.Error); ok {
			fmt.Printf("ERROR %s", e.Code)
			os.Exit(1)
		}
		fmt.Fprintf(os.Stderr, "internal error: %v\n", err)
		os.Exit(2)
	}

	os.Stdout.Write(encoded)
}
