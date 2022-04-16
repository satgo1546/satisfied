package main

import (
	"fmt"
	"io"
)

const (
	OpNoOp = iota
	OpReturn
	OpUnconditionalBranch
	OpBranchIfNonzero
	OpBranchIfPositive
	OpBranchIfNegative

	OpConst
	OpZeroExtend
	OpSignExtend

	OpNeg
	OpInc
	OpDec
	OpAdd
	OpSub
	OpCompare
	OpMul
	OpUDiv
	OpIDiv

	OpAnd
	OpOr
	OpXor
	OpNot
	OpShiftLeft
	OpRotateLeft
	OpLogicalShiftRight
	OpArithmeticShiftRight
	OpRotateRight
)

type Subroutine struct {
	Name string
	Args []any
	Code []Instruction
}

type Instruction struct {
	Opcode int
	Args   []int
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

func emit_subroutine(subroutine *Subroutine) {
	emit_noindent("%s:", subroutine.Name)
	emit("push ebp")
	emit("mov ebp, esp")
	emit("sub esp, %d*4", len(subroutine.Code))
	for i, inst := range subroutine.Code {
		emit_noindent(".L%d: ; %+v", i, inst)
		switch inst.Opcode {
		case OpReturn:
			emit("mov eax, [esp+%d*4]", inst.Args[0])
			emit("leave")
			emit("ret")
		case OpConst:
			emit("mov dword [esp+%d*4], %d", i, inst.Args[0])
		case OpMul:
			emit("mov eax, [esp+%d*4]", inst.Args[0])
			emit("imul eax, [esp+%d*4]", inst.Args[1])
			emit("mov [esp+%d*4], eax", i)
		}
	}
}
