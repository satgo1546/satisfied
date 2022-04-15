package main

import (
	"fmt"
	"io"
)

const (
	OpNil = iota
	OpReturn
	OpBranch
	OpConditionalBranch

	OpSet
	OpZeroExtend
	OpSignExtend

	OpNeg
	OpInc
	OpDec
	OpAdd
	OpSub
	OpMul
	OpUDiv
	OpIDiv

	OpAnd
	OpOr
	OpXor
	OpNot
	OpShiftLeft
	OpLogicalShiftRight
	OpArithmeticShiftRight

	OpEq
	OpGT
	OpGE
	OpLT
	OpLE
	OpNE
)

type Subroutine struct {
	Name   string
	Args   []any
	Locals []any
	Blocks [][]Instruction
}

type Instruction struct {
	Opcode      int
	Destination int
	Args        []int
}

var outputfp io.Writer

func emit(format string, a ...any) {
	emit_noindent("\t"+format, a...)
}
func emit_noindent(format string, a ...any) {
	fmt.Fprintf(outputfp, format, a...)
	outputfp.Write([]byte{'\n'})
}

func Align(n int, m int) int {
	rem := n % m
	if rem == 0 {
		return n
	}
	return n - rem + m
}

func emit_instruction(inst *Instruction) {
	switch inst.Opcode {
	case OpReturn:
		emit("mov eax, [esp+%d*4]", inst.Args[0])
		emit("leave")
		emit("ret")
	case OpBranch:
	case OpSet:
		emit("mov dword [esp+%d*4], %d", inst.Destination, inst.Args[0])
	case OpMul:
		emit("mov eax, [esp+%d*4]", inst.Args[0])
		emit("imul eax, [esp+%d*4]", inst.Args[1])
		emit("mov [esp+%d*4], eax", inst.Destination)
	}
}

func emit_block(block []Instruction) {
	emit_noindent(".blk%p:", block)
	for _, i := range block {
		emit_instruction(&i)
	}
}

func emit_subroutine(subroutine *Subroutine) {
	emit_noindent("%s:", subroutine.Name)
	emit("push ebp")
	emit("mov ebp, esp")
	emit("sub esp, %d*4", len(subroutine.Locals))
	for _, b := range subroutine.Blocks {
		emit_block(b)
	}
}
