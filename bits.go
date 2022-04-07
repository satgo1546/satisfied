package main

import (
	"encoding/binary"
	"io"
)

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

func writestrn(f io.Writer, str string, n int) {
	assert(len(str) <= n)
	f.Write([]byte(str))
	for i := len(str); i < n; i++ {
		write8(f, 0)
	}
}

// Write a string to file, padded to 8 bytes.
func writestr8(f io.Writer, str string) {
	writestrn(f, str, 8)
}

// Write a string to file, null-terminated and padded to 20 bytes.
func writestr20(f io.Writer, str string) {
	writestrn(f, str, 20)
}

func align_to(value int64, alignment int64) int64 {
	value += alignment - 1
	value -= value % alignment
	return value
}
