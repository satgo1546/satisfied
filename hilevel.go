package main

import (
	"encoding/json"
	"fmt"
	"io"
)

type Node struct {
	Type           string      `json:"type"`
	ReferenceLevel int         `json:"refl" nodeTypes:"ref break"`
	ID             int         `json:"refn" nodeTypes:"var ref"`
	Name           string      `json:"name" nodeTypes:"var builtin"`
	Description    string      `json:"desc" nodeTypes:"var"`
	Arguments      []*Node     `json:"args" nodeTypes:"scope call"`
	Condition      *Node       `json:"cond" nodeTypes:"if while"`
	Then           *Node       `json:"then" nodeTypes:"if while"`
	Else           *Node       `json:"else" nodeTypes:"if while"`
	Head           *Node       `json:"head" nodeTypes:"call"`
	LValue         *Node       `json:"lval" nodeTypes:"assign"`
	RValue         *Node       `json:"rval" nodeTypes:"scope assign break"`
	Immediate      interface{} `json:"ival" nodeTypes:"literal"`
	referenceLevel *Node
	reference      *Node
	definitionMap  map[int]*Node
	currentValue   *Instruction
}

func IsShortNode(node *Node) bool {
	if node == nil {
		return true
	}
	switch node.Type {
	case "literal", "ref", "builtin":
		return true
	}
	return false
}

func WriteField(f io.Writer, name string, value any) {
	fmt.Fprintf(f, ",\"%s\":", name)
	switch v := value.(type) {
	case *Node:
		if !IsShortNode(v) {
			fmt.Fprintln(f)
		}
		WriteNode(f, v)
	case []*Node:
		fmt.Fprintln(f)
		if len(v) != 0 {
			for i, node := range v {
				ch := ','
				if i == 0 {
					ch = '['
				}
				if IsShortNode(node) {
					fmt.Fprintf(f, "%-8c", ch)
				} else {
					fmt.Fprintf(f, "%c\n", ch)
				}
				WriteNode(f, node)
			}
		} else {
			fmt.Fprintln(f, "[")
		}
		fmt.Fprintln(f, "]")
	default:
		// The error value is thrown away.
		bytes, _ := json.Marshal(v)
		fmt.Fprintf(f, "%s\n", bytes)
	}
}

func WriteNode(f io.Writer, node *Node) {
	if node == nil {
		fmt.Fprintln(f, "null")
		return
	}
	switch node.Type {
	case "literal":
		fmt.Fprintf(f, `{"type": "literal", "ival": %#v`, node.Immediate)
	case "ref":
		fmt.Fprintf(f, `{"type": "ref", "refl": %d, "refn": %d`, node.ReferenceLevel, node.ID)
	case "builtin":
		fmt.Fprintf(f, `{"type": "builtin", "name": %q`, node.Name)
	default:
		fmt.Fprintf(f, "{\"type\":\"%s\"\n", node.Type)
		switch node.Type {
		case "var":
			WriteField(f, "refn", node.ID)
			WriteField(f, "name", node.Name)
			WriteField(f, "desc", node.Description)
		case "scope":
			WriteField(f, "args", node.Arguments)
			WriteField(f, "rval", node.RValue)
		case "if", "while":
			WriteField(f, "cond", node.Condition)
			WriteField(f, "then", node.Then)
			WriteField(f, "else", node.Else)
		case "call":
			WriteField(f, "head", node.Head)
			WriteField(f, "args", node.Arguments)
		case "assign":
			WriteField(f, "lval", node.LValue)
			WriteField(f, "rval", node.RValue)
		case "break":
			WriteField(f, "refl", node.ReferenceLevel)
			WriteField(f, "rval", node.RValue)
		}
	}
	fmt.Fprintln(f, "}")
}

// Retrocycle finds ref nodes and turn numeric references (ReferenceLevel and ID) into pointers (referenceLevel and reference).
// Other nodes with ReferenceLevel set (e.g., break nodes) are also updated.
func (node *Node) Retrocycle(definitionStack []*Node) {
	if node == nil {
		return
	}
	node.definitionMap = nil
	if node.Type == "scope" {
		node.definitionMap = make(map[int]*Node, len(node.Arguments))
		for _, d := range node.Arguments {
			if d.ID < 0 {
				panic("negative refn reserved")
			}
			if node.definitionMap[d.ID] != nil {
				panic("repeated refn defined")
			}
			node.definitionMap[d.ID] = d
		}
		definitionStack = append(definitionStack, node)
	}
	node.referenceLevel = nil
	node.reference = nil
	if node.ReferenceLevel != 0 {
		// The hierarchy of the definition stack eventually gets unused in analysis stages.
		// Scopes in the source are meant for programmers. They play the rôle of self-explanatory comments.
		// Reference numbers are adapted to a global numbering scheme (a.k.a. pointers) here.
		node.referenceLevel = definitionStack[len(definitionStack)+node.ReferenceLevel]
		if node.ID != 0 {
			node.reference = node.referenceLevel.definitionMap[node.ID]
		}
		// From now on, the pointer fields are the source of truth.
		node.ReferenceLevel = 0
		node.ID = 0
	}
	for _, n := range append(
		node.Arguments,
		node.Condition,
		node.Then,
		node.Else,
		node.Head,
		node.LValue,
		node.RValue,
	) {
		n.Retrocycle(definitionStack)
	}
}

func (node *Node) DesugarBreak() bool {
	return false
}

func (node *Node) Compile(
	beforeAssignment func(l *Node, r *Instruction),
) (
	head *Instruction,
	result *Instruction,
) {
	if node == nil {
		head = &Instruction{Opcode: OpConst, Const: 0}
		result = head
		return
	}
	switch node.Type {
	case "literal":
		head = &Instruction{Opcode: OpConst, Const: int(node.Immediate.(float64))}
		result = head
	case "ref":
		result = node.reference.currentValue
	case "builtin":
		switch node.Name {
		case "add":
		}
		panic("not implemented: builtins can only be called for now")
	case "scope":
		head = &Instruction{Opcode: OpConst, Const: 0}
		tail := head
		for _, d := range node.Arguments {
			tail.Next = (&Instruction{Opcode: OpCopy, Arg0: head}).RegisterUses()
			tail = tail.Next
			d.currentValue = tail
		}
		tail.Next, result = node.RValue.Compile(beforeAssignment)
	case "if":
		head, result = node.Condition.Compile(beforeAssignment)
		i := (&Instruction{Opcode: OpIfNonzero, Arg0: result}).RegisterUses()
		head = AppendInstructions(head, i)
		result = &Instruction{Opcode: OpΦ}
		φs := make(map[*Node]*Instruction)
		i.List0, result.Arg0 = node.Then.Compile(func(l *Node, r *Instruction) {
			if φs[l] != nil {
				φs[l].Arg0 = r
			} else {
				φs[l] = &Instruction{Opcode: OpΦ, Arg0: r, Arg1: l.currentValue, Arg2: l.currentValue}
			}
		})
		for d, φ := range φs {
			d.currentValue = φ.Arg2
		}
		i.List1, result.Arg1 = node.Else.Compile(func(l *Node, r *Instruction) {
			if φs[l] != nil {
				φs[l].Arg1 = r
			} else {
				φs[l] = &Instruction{Opcode: OpΦ, Arg0: l.currentValue, Arg1: r, Arg2: l.currentValue}
			}
		})
		result.RegisterUses()
		i.Next = result
		for d, φ := range φs {
			d.currentValue = φ.Arg2
			φ.Arg2 = nil
			φ.RegisterUses()
			if beforeAssignment != nil {
				beforeAssignment(d, φ)
			}
			d.currentValue = φ
			φ.Next = i.Next
			i.Next = φ
		}
	case "while":
		// At the time a new OpΦ is created, there may already be accesses to the variable within the loop, refering to a definition somewhere outside, which must be amended.
		// It is infeasible to replace all uses of the old value blindly, because
		// • the value can be shared between different variables, and
		// • there can be uses outside the loop, which must not change.
		// The former is remedied by requiring that two variables can never refer to the same defining instruction. This can be accomplished by giving variables different initial values and inserting redundant OpCopy instructions at assignments.
		// 10.1145/197320.197331 solves the latter by allocating instruction objects in contiguous memory so that their addresses increase monotonically. Uses outside are then easily distinguished by address ≶ the loop instruction.
		// eth11024 does a dominance test to determine whether an instruction is inside the loop. I don't know how this is done exactly as the dominance test requires a built-up tree to work efficiently.
		// Here, I tag each instruction with a serial number in RegisterUses. Outsiders are rejected through a simple comparison.
		head = (&Instruction{
			Opcode: OpWhile,
			Arg0:   &Instruction{Opcode: OpIfNonzero},
		}).RegisterUses()
		φs := make(map[*Node]*Instruction)
		h := func(l *Node, r *Instruction) {
			if φs[l] != nil {
				φs[l].Arg0 = r
			} else {
				φs[l] = &Instruction{Opcode: OpΦ, Arg0: r, Arg1: l.currentValue}
				l.currentValue.ReplaceUsesWithMinSerial(φs[l], head.Serial)
			}
		}
		head.List0, head.Arg0.Arg0 = node.Condition.Compile(h)
		head.List0 = AppendInstructions(head.List0, head.Arg0.RegisterUses())
		head.Arg0.List0, _ = node.Then.Compile(h)
		result = &Instruction{Opcode: OpConst, Const: 0}
		head.Next = result
		for d, φ := range φs {
			φ.RegisterUses()
			d.currentValue = φ.Arg1
			if beforeAssignment != nil {
				beforeAssignment(d, φ)
			}
			d.currentValue = φ
			φ.Next = head.List0
			head.List0 = φ
		}
	case "call":
		head = &Instruction{}
		tail := head
		args := make([]*Instruction, len(node.Arguments))
		for i, a := range node.Arguments {
			tail.Next, args[i] = a.Compile(beforeAssignment)
			for ; tail.Next != nil; tail = tail.Next {
			}
		}
		if node.Head.Type == "builtin" {
			switch node.Head.Name {
			case "arglast":
				result = args[len(args)-1]
			case "add", "sub", "mul", "bitand", "bitor", "bitxor":
				tail.Next = (&Instruction{
					Opcode: map[string]int{
						"add":    OpAdd,
						"sub":    OpSub,
						"mul":    OpMul,
						"bitand": OpAnd,
						"bitor":  OpOr,
						"bitxor": OpXor,
					}[node.Head.Name],
					Arg0: args[0],
					Arg1: args[1],
				}).RegisterUses()
				result = tail.Next
			case "eq", "neq", "lt", "gt", "leq", "geq":
				tail.Next = (&Instruction{Opcode: OpICompare, Arg0: args[0], Arg1: args[1]}).RegisterUses()
				tail.Next.Next = (&Instruction{
					Opcode: map[string]int{
						"eq":  OpIfZero,
						"neq": OpIfNonzero,
						"lt":  OpIfNegative,
						"gt":  OpIfPositive,
						"leq": OpIfNonpositive,
						"geq": OpIfNonnegative,
					}[node.Head.Name],
					Arg0:  tail.Next,
					List0: &Instruction{Opcode: OpConst, Const: 1},
					List1: &Instruction{Opcode: OpConst, Const: 0},
				}).RegisterUses()
				tail.Next.Next.Next = (&Instruction{
					Opcode: OpΦ,
					Arg0:   tail.Next.Next.List0,
					Arg1:   tail.Next.Next.List1,
				}).RegisterUses()
				result = tail.Next.Next.Next
			}
		} else {
			panic("not implemented: only builtins can be called for now")
		}
		head = head.Next
	case "assign":
		switch node.LValue.Type {
		case "ref":
			l := node.LValue.reference
			head, result = node.RValue.Compile(beforeAssignment)
			// An OpCopy is mandated at each assignment so as to compile while nodes efficiently.
			result = (&Instruction{Opcode: OpCopy, Arg0: result}).RegisterUses()
			head = AppendInstructions(head, result)
			if beforeAssignment != nil {
				beforeAssignment(l, result)
			}
			l.currentValue = result
		default:
			panic("non-lvalue in assignment lhs")
		}
	default:
		panic("unknown node type")
	}
	return
}
