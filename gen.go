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
	Serial int
	Opcode int
	Arg0   *Instruction
	Arg1   *Instruction
	Arg2   *Instruction
	Arg3   *Instruction
	Const  int
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

func NumberInstructions(inst *Instruction, start int) int {
	for ; inst != nil; inst = inst.Next {
		inst.Serial = start
		start++
		switch inst.Opcode {
		case OpIfNonzero, OpIfPositive, OpIfNegative:
			start = NumberInstructions(inst.Arg0, start)
			start = NumberInstructions(inst.Arg1, start)
			start = NumberInstructions(inst.Arg2, start)
		case OpWhile:
			start = NumberInstructions(inst.Arg0, start)
		}
	}
	return start
}

func emit_instructions(inst *Instruction, pred0 *Instruction) {
	for ; inst != nil; inst = inst.Next {
		emit_noindent(".L%d: ; %p = %+v", inst.Serial, inst, inst)
		switch inst.Opcode {
		case OpΦ:
			emit("mov eax, [esp+%d*4]", inst.Arg0.Serial)
			emit("test dword [esp+%d*4], 1", pred0.Serial)
			emit("cmovz eax, [esp+%d*4]", inst.Arg1.Serial)
			emit("mov [esp+%d*4], eax", inst.Serial)
		case OpReturn:
			emit("mov eax, [esp+%d*4]", inst.Arg0.Serial)
			emit("leave")
			emit("ret")
		case OpIfNonzero, OpIfPositive, OpIfNegative:
			emit("cmp dword [esp+%d*4], 0", inst.Arg3.Serial)
			emit("%s .L%d", map[int]string{
				OpIfNonzero:  "je",
				OpIfPositive: "jle",
				OpIfNegative: "jge",
			}[inst.Opcode], inst.Arg1.Serial)
			emit_instructions(inst.Arg0, nil)
			emit("mov dword [esp+%d*4], 1", inst.Serial)
			emit("jmp .L%d", inst.Arg2.Serial)
			emit_instructions(inst.Arg1, nil)
			emit("mov dword [esp+%d*4], 0", inst.Serial)
			emit_instructions(inst.Arg2, inst)
		case OpWhile:
			// TODO
		case OpConst:
			emit("mov dword [esp+%d*4], %d", inst.Serial, inst.Const)
		case OpNot, OpNeg:
			emit("mov eax, [esp+%d*4]", inst.Arg0.Serial)
			emit("%s eax", map[int]string{
				OpNot: "not",
				OpNeg: "neg",
			}[inst.Opcode])
			emit("mov [esp+%d*4], eax", inst.Serial)
		case OpAdd, OpSub, OpMul, OpAnd, OpOr, OpXor:
			emit("mov eax, [esp+%d*4]", inst.Arg0.Serial)
			emit("%s eax, [esp+%d*4]", map[int]string{
				OpAdd: "add",
				OpSub: "sub",
				OpMul: "imul",
				OpAnd: "and",
				OpOr:  "or",
				OpXor: "xor",
			}[inst.Opcode], inst.Arg1.Serial)
			emit("mov [esp+%d*4], eax", inst.Serial)
		}
	}
}

func emit_subroutine(subroutine *Subroutine) {
	emit_noindent("%s:", subroutine.Name)
	// Reserve space appropriately first.
	// https://devblogs.microsoft.com/oldnewthing/20190111-00/?p=100685
	emit("push ebp")
	emit("mov ebp, esp")
	emit("sub esp, %d*4", NumberInstructions(subroutine.Code, 0))
	emit_instructions(subroutine.Code, nil)
}
