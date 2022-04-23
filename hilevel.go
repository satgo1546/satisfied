package main

import (
	"encoding/json"
	"fmt"
	"io"
)

type Node struct {
	Type           string      `json:"type"`
	ReferenceLevel int         `json:"refl" nodeTypes:"ref"`
	ID             int64       `json:"refn" nodeTypes:"var ref"`
	Name           string      `json:"name" nodeTypes:"var builtin"`
	Description    string      `json:"desc" nodeTypes:"var"`
	Definitions    []*Node     `json:"defs" nodeTypes:"block"`
	Body           []*Node     `json:"body" nodeTypes:"block"`
	Condition      *Node       `json:"cond" nodeTypes:"if while"`
	Then           *Node       `json:"then" nodeTypes:"if while"`
	Else           *Node       `json:"else" nodeTypes:"if while"`
	Head           *Node       `json:"head" nodeTypes:"call"`
	Arguments      []*Node     `json:"args" nodeTypes:"call"`
	LValue         *Node       `json:"lval" nodeTypes:"assign"`
	RValue         *Node       `json:"rval" nodeTypes:"assign return"`
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
			WriteField(f, "body", node.Body)
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
