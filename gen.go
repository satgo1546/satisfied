package main

import (
	"fmt"
	"io"
)

const (
	OpNoOp = iota

	// OpΦ.Arg2 is transient and acts as a “save stack” in TeX lingo, so that the compiler can roll back assignments after a control structure.
	OpΦ
	OpIfNonzero
	OpIfZero
	OpIfNonpositive
	OpIfPositive
	OpIfNonnegative
	OpIfNegative
	OpWhile

	OpConst
	OpCopy
	OpZeroExtend
	OpSignExtend

	OpNeg
	OpAdd
	OpSub
	// The spaceship operator <=> in Ruby and C++20.
	OpUCompare
	OpICompare
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
	Ret  *Instruction
}

// There are no three-operand instructions for now.
// Arg2 is reserved for future operations like fused multiply-add.
// vpblendd in x86 AVX2 has four operands, but the first serves only as the destination.
type Instruction struct {
	Next   *Instruction
	Info   int
	Serial int
	Opcode int
	Arg0   *Instruction
	Arg1   *Instruction
	Arg2   *Instruction
	List0  *Instruction
	List1  *Instruction
	Const  int
	Uses   map[*Instruction]bool
}

var outputfp io.Writer

func emit(format string, a ...any) {
	outputfp.Write([]byte{'\t'})
	emit_noindent(format, a...)
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

var nextInstructionSerial int

func PrintInstructions(f io.Writer, inst *Instruction, indent int, ch rune) {
	if inst == nil {
		return
	}
	fmt.Fprintf(f, "%*cL%d: Op%d", indent, ch, inst.Serial, inst.Opcode)
	if inst.Arg0 != nil {
		fmt.Fprintf(f, ", %d", inst.Arg0.Serial)
		if inst.Arg1 != nil {
			fmt.Fprintf(f, ", %d", inst.Arg1.Serial)
			if inst.Arg2 != nil {
				fmt.Fprintf(f, ", %d", inst.Arg2.Serial)
			}
		}
	}
	if inst.Opcode == OpConst {
		fmt.Fprintf(f, "=OpConst %d", inst.Const)
	}
	if len(inst.Uses) != 0 {
		fmt.Fprintf(f, ", used in")
		for i := range inst.Uses {
			fmt.Fprintf(f, " %d", i.Serial)
		}
	}
	fmt.Fprintf(f, "\n")
	PrintInstructions(f, inst.List0, indent+1, '.')
	PrintInstructions(f, inst.List1, indent+1, ':')
	PrintInstructions(f, inst.Next, indent, ch)
}

// A chaining method (i.e., that returns self) to append self to the use chain of the arguments.
// It is obvious that maintenance of a use chain must be done manually if no getters or setters are introduced.
// Since an OpConst uses nothing, there's no need to call RegisterUses on an OpConst.
func (inst *Instruction) RegisterUses() *Instruction {
	for _, arg := range []*Instruction{inst.Arg0, inst.Arg1, inst.Arg2} {
		if arg != nil {
			if arg.Uses == nil {
				arg.Uses = make(map[*Instruction]bool)
			}
			arg.Uses[inst] = true
		}
	}
	// Number the instruction in RegisterUses, for use in compilation of while nodes.
	if inst.Serial == 0 {
		nextInstructionSerial++
		inst.Serial = nextInstructionSerial
	}
	return inst
}

func (inst *Instruction) UnregisterUses() {
	if inst.Arg0 != nil {
		delete(inst.Arg0.Uses, inst)
	}
	if inst.Arg1 != nil {
		delete(inst.Arg1.Uses, inst)
	}
	if inst.Arg2 != nil {
		delete(inst.Arg2.Uses, inst)
	}
}

func (old *Instruction) ReplaceUsesWithMinSerial(new *Instruction, minSerial int) {
	for inst := range old.Uses {
		if inst.Serial >= minSerial {
			inst.UnregisterUses()
			if inst.Arg0 == old {
				inst.Arg0 = new
			}
			if inst.Arg1 == old {
				inst.Arg1 = new
			}
			if inst.Arg2 == old {
				inst.Arg2 = new
			}
			inst.RegisterUses()
		}
	}
}

func (old *Instruction) ReplaceUses(new *Instruction) {
	old.ReplaceUsesWithMinSerial(new, 0)
}

// AppendInstructions works like append (the Go built-in function for slices), in that the return value has to be assigned back, in the case of singly linked lists without a dedicated head node.
// This function is O(n) in time.
func AppendInstructions(base *Instruction, extension *Instruction) *Instruction {
	if base == nil {
		return extension
	}
	for tail := base; ; tail = tail.Next {
		if tail.Next == nil {
			tail.Next = extension
			return base
		}
	}
}

func RenumberInstructions(inst *Instruction, start int) int {
	for ; inst != nil; inst = inst.Next {
		inst.Serial = start
		start++
		start = RenumberInstructions(inst.List0, start)
		start = RenumberInstructions(inst.List1, start)
	}
	return start
}

func EmitInstructions(
	inst *Instruction,
	pred0 *Instruction, // controls OpΦ
	backedges map[*Instruction]*Instruction, // Arg0 (an OpIf) ↦ OpWhile
) {
	// SSA φ-functions must be executed simultaneously if interpreted.
	// Reference: Interpreting programs in static single assignment form by Jeffery von Ronne, Ning Wang, and Michael Franz.
	φ := 0
	for i := inst; i != nil && i.Opcode == OpΦ; i = i.Next {
		φ++
	}
	if φ != 0 {
		i0 := inst
		emit_noindent(".L%d: ; %d φ instruction(s)", inst.Serial, φ)
		emit("sub esp, %d*4", φ)
		for j := 0; j < φ; j++ {
			emit("; %p = %+v", inst, inst)
			emit("mov eax, [esp+(%d+%d)*4]", inst.Arg0.Serial, φ)
			emit("test dword [esp+(%d+%d)*4], 1", pred0.Serial, φ)
			emit("cmovz eax, [esp+(%d+%d)*4]", inst.Arg1.Serial, φ)
			emit("mov [esp+%d*4], eax", j)
			inst = inst.Next
		}
		inst = i0
		for j := 0; j < φ; j++ {
			emit("mov eax, [esp+%d*4]", j)
			emit("mov [esp+(%d+%d)*4], eax", inst.Serial, φ)
			inst = inst.Next
		}
		emit("add esp, %d*4", φ)
	}
	for ; inst != nil; inst = inst.Next {
		emit_noindent(".L%d: ; %p = %+v", inst.Serial, inst, inst)
		switch inst.Opcode {
		case OpΦ:
			panic("OpΦ in the middle of a block")
		case OpIfNonzero, OpIfZero, OpIfNonpositive, OpIfPositive, OpIfNonnegative, OpIfNegative:
			// L%d_then, L%d_else and L%d_end labels are necessary since empty bodies have no instruction numbers themselves.
			emit("cmp dword [esp+%d*4], 0", inst.Arg0.Serial)
			emit("%s .L%d_else", map[int]string{
				OpIfNonzero:     "je",
				OpIfZero:        "jne",
				OpIfNonpositive: "jg",
				OpIfPositive:    "jle",
				OpIfNonnegative: "jl",
				OpIfNegative:    "jge",
			}[inst.Opcode], inst.Serial)

			emit_noindent(".L%d_then:", inst.Serial)
			emit("mov dword [esp+%d*4], 1", inst.Serial)
			EmitInstructions(inst.List0, nil, backedges)
			if backedges[inst] != nil {
				emit("mov dword [esp+%d*4], 1", backedges[inst].Serial)
				emit("jmp .L%d_loop", backedges[inst].Serial)
				delete(backedges, inst)
			} else {
				emit("jmp .L%d_end", inst.Serial)
			}

			emit_noindent(".L%d_else:", inst.Serial)
			emit("mov dword [esp+%d*4], 0", inst.Serial)
			EmitInstructions(inst.List1, nil, backedges)

			emit_noindent(".L%d_end:", inst.Serial)
			EmitInstructions(inst.Next, inst, backedges)
			return
		case OpWhile:
			emit("mov dword [esp+%d*4], 0", inst.Serial)
			emit_noindent(".L%d_loop:", inst.Serial)
			if backedges[inst.Arg0] != nil {
				panic("condition already used by another OpWhile")
			}
			backedges[inst.Arg0] = inst
			EmitInstructions(inst.List0, inst, backedges)
		case OpConst:
			emit("mov dword [esp+%d*4], %d", inst.Serial, inst.Const)
		case OpCopy:
			emit("mov eax, [esp+%d*4]", inst.Arg0.Serial)
			emit("mov [esp+%d*4], eax", inst.Serial)
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
		case OpUCompare, OpICompare:
			emit("mov eax, [esp+%d*4]", inst.Arg0.Serial)
			emit("cmp eax, [esp+%d*4]", inst.Arg1.Serial)
			emit("set%c ch", "ag"[inst.Opcode-OpUCompare])
			emit("set%c cl", "bl"[inst.Opcode-OpUCompare])
			emit("sub ch, cl")
			emit("movsx eax, ch")
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
	emit("sub esp, %d*4", RenumberInstructions(subroutine.Code, 0))
	EmitInstructions(subroutine.Code, nil, make(map[*Instruction]*Instruction))
	emit("mov eax, [esp+%d*4]", subroutine.Ret.Serial)
	emit("leave")
	emit("ret")
}
