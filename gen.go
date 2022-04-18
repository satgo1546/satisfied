package main

import (
	"fmt"
	"io"
)

const (
	OpNoOp = iota

	// OpReturn doesn't seem to fit here...
	OpReturn

	OpΦ
	OpIfNonzero
	OpIfPositive
	OpIfNegative
	OpWhile

	OpConst
	OpZeroExtend
	OpSignExtend

	OpNeg
	OpAdd
	OpSub
	// The spaceship operator <=> in Ruby and C++20.
	OpCompare
	OpMul
	OpUDiv
	OpIDiv
	OpUMod
	OpIMod

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
	Code *Instruction
}

type Instruction struct {
	Next   *Instruction
	Info   int
	Opcode int
	Arg0   *Instruction
	Arg1   *Instruction
	Const  int
	Ptr    *Instruction
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

var lastID int

func (inst *Instruction) ID() int {
	if inst.Info == 0 {
		lastID++
		inst.Info = lastID
	}
	return inst.Info
}

func emit_subroutine(subroutine *Subroutine) {
	emit_noindent("%s:", subroutine.Name)
	for inst := subroutine.Code; inst != nil; inst = inst.Next {
		emit_noindent(".L%d: ; %p = %+v", inst.ID(), inst, inst)
		switch inst.Opcode {
		case OpΦ:
			// TODO
			emit("mov eax, [esp-%d*4-4]", inst.Arg0.ID())
			emit("cmovz eax, [esp-%d*4-4]", inst.Arg1.ID())
			emit("mov [esp-%d*4], eax", inst.Info)
		case OpReturn:
			emit("mov eax, [esp-%d*4-4]", inst.Arg0.Info)
			emit("ret")
		case OpIfNonzero:
			// TODO
			emit("cmp dword [esp-%d*4-4], 0", inst.Arg0.ID())
			emit("jne .BB%d", inst.Arg1.ID())
		case OpConst:
			emit("mov dword [esp-%d*4-4], %d", inst.ID(), inst.Const)
		case OpNot, OpNeg:
			emit("mov eax, [esp-%d*4-4]", inst.Arg0.Info)
			emit("%s eax", map[int]string{
				OpNot: "not",
				OpNeg: "neg",
			}[inst.Opcode])
			emit("mov [esp-%d*4-4], eax", inst.Info)
		case OpAdd, OpSub, OpMul, OpAnd, OpOr, OpXor:
			emit("mov eax, [esp-%d*4-4]", inst.Arg0.Info)
			emit("%s eax, [esp-%d*4-4]", map[int]string{
				OpAdd: "add",
				OpSub: "sub",
				OpMul: "imul",
				OpAnd: "and",
				OpOr:  "or",
				OpXor: "xor",
			}[inst.Opcode], inst.Arg1.Info)
			emit("mov [esp-%d*4-4], eax", inst.Info)
		}
	}
}
