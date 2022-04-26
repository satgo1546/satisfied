package main

import (
	"encoding/json"
	"fmt"
	"io"
)

type Node struct {
	Type           string      `json:"type"`
	ReferenceLevel int         `json:"refl" nodeTypes:"ref"`
	ID             int         `json:"refn" nodeTypes:"var ref"`
	Name           string      `json:"name" nodeTypes:"var builtin"`
	Description    string      `json:"desc" nodeTypes:"var"`
	Definitions    []*Node     `json:"defs" nodeTypes:"block"`
	Condition      *Node       `json:"cond" nodeTypes:"if while"`
	Then           *Node       `json:"then" nodeTypes:"if while"`
	Else           *Node       `json:"else" nodeTypes:"if while"`
	Head           *Node       `json:"head" nodeTypes:"call"`
	Arguments      []*Node     `json:"args" nodeTypes:"call"`
	LValue         *Node       `json:"lval" nodeTypes:"assign"`
	RValue         *Node       `json:"rval" nodeTypes:"block assign return"`
	Immediate      interface{} `json:"ival" nodeTypes:"literal"`
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
		fmt.Fprintf(f, `{"type": "ref", "refn": %d, "refl": %d`, node.ID, node.ReferenceLevel)
	case "builtin":
		fmt.Fprintf(f, `{"type": "builtin", "name": %q`, node.Name)
	default:
		fmt.Fprintf(f, "{\"type\":\"%s\"\n", node.Type)
		switch node.Type {
		case "var":
			WriteField(f, "refn", node.ID)
			WriteField(f, "name", node.Name)
			WriteField(f, "desc", node.Description)
		case "block":
			WriteField(f, "defs", node.Definitions)
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
		case "return":
			WriteField(f, "rval", node.RValue)
		}
	}
	fmt.Fprintln(f, "}")
}

type defItem struct {
	node         *Node
	currentValue *Instruction
}

func CompileNode(node *Node, definitionStack []map[int]defItem) (head, result *Instruction) {
	if node == nil {
		head = &Instruction{Opcode: OpConst, Const: 0}
		return head, head
	}
	switch node.Type {
	case "literal":
		head = &Instruction{Opcode: OpConst, Const: int(node.Immediate.(float64))}
		result = head
	case "ref":
		return nil, definitionStack[len(definitionStack)-1+node.ReferenceLevel][node.ID].currentValue
	case "builtin":
		switch node.Name {
		case "add":
		}
	case "block":
		head = &Instruction{Opcode: OpConst, Const: 0}
		defs := make(map[int]defItem, len(node.Definitions))
		for _, d := range node.Definitions {
			if defs[d.ID].node != nil {
				panic("repeated refn defined")
			}
			defs[d.ID] = defItem{
				node:         d,
				currentValue: head,
			}
		}
		head.Next, result = CompileNode(node.RValue, append(definitionStack, defs))
	case "if":
		// TODO
	case "while":
		// TODO
	case "call":
		head = &Instruction{}
		tail := head
		args := make([]*Instruction, len(node.Arguments))
		for i, a := range node.Arguments {
			tail.Next, args[i] = CompileNode(a, definitionStack)
			for ; tail.Next != nil; tail = tail.Next {
			}
		}
		if node.Head.Type == "builtin" {
			switch node.Head.Name {
			case "arglast":
				result = args[len(args)-1]
			case "mul":
				tail.Next = &Instruction{Opcode: OpMul, Arg0: args[0], Arg1: args[1]}
				result = tail.Next
			case "gt":
				// TODO
			}
		}
		head = head.Next
	case "assign":
		// TODO
	}
	return
}
