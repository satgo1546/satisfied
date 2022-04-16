package main

import (
	"fmt"
	"io"
)

const (
	OpNoOp = iota
	OpReturn
	OpBranch
	OpBranchIfNonzero
	OpBranchIfPositive
	OpBranchIfNegative
	OpΦ

	OpConst
	OpZeroExtend
	OpSignExtend

	OpNeg
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
	emit_noindent(".BB0:")
	b := 0
	for i, inst := range subroutine.Code {
		emit_noindent(".L%d: ; %+v", i, inst)
		switch inst.Opcode {
		case OpReturn:
			emit("mov eax, [esp+%d*4]", inst.Args[0])
			emit("leave")
			emit("ret")
			b++
			emit_noindent(".BB%d:", b)
		case OpBranch:
			emit("mov dl, %d", inst.Args[1])
			emit("jmp .BB%d", inst.Args[0])
			b++
			emit_noindent(".BB%d:", b)
		case OpBranchIfNonzero:
			emit("cmp dword [esp+%d*4], 0", inst.Args[0])
			emit("mov dl, %d", inst.Args[2])
			emit("jne .BB%d", inst.Args[1])
			emit("mov dl, %d", inst.Args[4])
			emit("jmp .BB%d", inst.Args[3])
			b++
			emit_noindent(".BB%d:", b)
		case OpΦ:
			if len(inst.Args) != 0 {
				emit("mov eax, [esp+%d*4]", inst.Args[0])
				for _, edge := range inst.Args[1:] {
					emit("dec dl")
					emit("cmovz eax, [esp+%d*4]", edge)
				}
				emit("mov [esp+%d*4], eax", i)
			}
		case OpConst:
			emit("mov dword [esp+%d*4], %d", i, inst.Args[0])
		case OpSub:
			emit("mov eax, [esp+%d*4]", inst.Args[0])
			emit("sub eax, [esp+%d*4]", inst.Args[1])
			emit("mov [esp+%d*4], eax", i)
		case OpMul:
			emit("mov eax, [esp+%d*4]", inst.Args[0])
			emit("imul eax, [esp+%d*4]", inst.Args[1])
			emit("mov [esp+%d*4], eax", i)
		}
	}
}
