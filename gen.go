package main

import (
	"fmt"
	"io"
)

const (
	OpReturn = iota
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
	Entry  *Block
}

type Block struct {
	Code []Instruction
	Next []*Block
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

func emit_block(visited map[*Block]bool, block *Block) {
	if visited[block] {
		return
	}
	visited[block] = true
	emit_noindent(".blk%p:", block)
	for _, i := range block.Code {
		emit_instruction(&i)
	}
	for _, b := range block.Next {
		emit_block(visited, b)
	}
}

func emit_subroutine(subroutine *Subroutine) {
	emit_noindent("%s:", subroutine.Name)
	emit("push ebp")
	emit("mov ebp, esp")
	emit("sub esp, %d*4", len(subroutine.Locals))
	emit_block(make(map[*Block]bool), subroutine.Entry)
}
