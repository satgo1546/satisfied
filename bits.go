package main

import (
	"encoding/binary"
	"io"
)

// I provide myself with this out of laziness.
// https://go.dev/doc/faq#assertions
func assert(predicate bool) {
	if !predicate {
		panic("assert")
	}
}

func write8(f io.Writer, value uint8) {
	f.Write([]byte{value})
}

func write16(f io.Writer, x uint16) {
	binary.Write(f, binary.LittleEndian, x)
}

func write32(f io.Writer, x uint32) {
	binary.Write(f, binary.LittleEndian, x)
}

// Write out a string, padded to n bytes.
func writestrn(f io.Writer, str string, n int) {
	assert(len(str) <= n)
	f.Write([]byte(str))
	for i := len(str); i < n; i++ {
		write8(f, 0)
	}
}

func align_to(value int64, alignment int64) int64 {
	value += alignment - 1
	value -= value % alignment
	return value
}
