package main

import (
	"fmt"
	"io"
	"os"
	"strconv"

	"github.com/Mentor82/EQUORUS/go/equorus"
)

func main() {
	if len(os.Args) != 3 && len(os.Args) != 7 {
		os.Exit(2)
	}

	limits := equorus.DefaultLimits()
	if len(os.Args) == 7 {
		mb, err1 := strconv.Atoi(os.Args[3])
		md, err2 := strconv.Atoi(os.Args[4])
		mi, err3 := strconv.Atoi(os.Args[5])
		ms, err4 := strconv.Atoi(os.Args[6])
		if err1 != nil || err2 != nil || err3 != nil || err4 != nil {
			os.Exit(2)
		}
		limits.MaxBytes = mb
		limits.MaxDepth = md
		limits.MaxItems = mi
		limits.MaxStringLength = ms
	}

	mode := os.Args[1]
	arg := os.Args[2]

	raw, err := io.ReadAll(os.Stdin)
	if err != nil {
		fmt.Fprintf(os.Stderr, "read error: %v\n", err)
		os.Exit(2)
	}
	if len(raw) > limits.MaxBytes {
		fmt.Printf("ERROR %s", equorus.ErrLimit)
		os.Exit(1)
	}

	codec := equorus.JsonCodec{}

	switch mode {
	case "canonical":
		val, err := codec.Decode(raw, limits)
		if err != nil {
			handleErr(err)
		}
		canon, err := equorus.CanonicalBytes(val, arg, limits)
		if err != nil {
			handleErr(err)
		}
		os.Stdout.Write(canon)

	case "hash":
		env, err := equorus.DecodePilot(raw, arg, limits)
		if err != nil {
			handleErr(err)
		}
		rec, err := equorus.ComputeIntegrity(env, equorus.CanonicalProfile, equorus.IntegrityAlgorithm, limits)
		if err != nil {
			handleErr(err)
		}
		enc, err := equorus.EncodeIntegrity(rec, limits)
		if err != nil {
			handleErr(err)
		}
		os.Stdout.Write(enc)

	case "record":
		rec, err := equorus.DecodeIntegrity(raw, limits)
		if err != nil {
			handleErr(err)
		}
		enc, err := equorus.EncodeIntegrity(rec, limits)
		if err != nil {
			handleErr(err)
		}
		os.Stdout.Write(enc)

	case "verify":
		reqVal, err := codec.Decode(raw, limits)
		if err != nil {
			handleErr(err)
		}
		if reqVal.Kind != equorus.KindObject {
			fmt.Printf("ERROR %s", equorus.ErrSchema)
			os.Exit(1)
		}
		envVal, ok1 := reqVal.Object["envelope"]
		intVal, ok2 := reqVal.Object["integrity"]
		if !ok1 || !ok2 {
			fmt.Printf("ERROR %s", equorus.ErrSchema)
			os.Exit(1)
		}
		env, err := equorus.CreatePilot(envVal, arg, limits)
		if err != nil {
			handleErr(err)
		}
		intBytes, err := codec.Encode(intVal, limits)
		if err != nil {
			handleErr(err)
		}
		rec, err := equorus.DecodeIntegrity(intBytes, limits)
		if err != nil {
			handleErr(err)
		}
		ok, err := equorus.VerifyIntegrity(env, rec, limits)
		if err != nil {
			handleErr(err)
		}
		if ok {
			os.Stdout.WriteString("true")
		} else {
			os.Stdout.WriteString("false")
		}

	default:
		os.Exit(2)
	}
}

func handleErr(err error) {
	if e, ok := err.(*equorus.Error); ok {
		fmt.Printf("ERROR %s", e.Code)
		os.Exit(1)
	}
	fmt.Fprintf(os.Stderr, "internal error: %v\n", err)
	os.Exit(2)
}
