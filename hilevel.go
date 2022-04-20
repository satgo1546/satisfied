package main

import (
	"encoding/json"
	"fmt"
	"io"
)

type Node struct {
	Type        string      `json:"type"`
	ID          int64       `json:"refn"`
	Name        string      `json:"name" stype:""`
	Description string      `json:"desc" stype:"var"`
	Definitions []*Node     `json:"defs" stype:"block"`
	Body        []*Node     `json:"body" stype:"block"`
	Condition   *Node       `json:"cond" stype:"if while"`
	Then        *Node       `json:"then" stype:"if while"`
	Else        *Node       `json:"else" stype:"if while"`
	Head        *Node       `json:"head" stype:"call"`
	Arguments   []*Node     `json:"args" stype:"call"`
	LValue      *Node       `json:"lval" stype:"assign"`
	RValue      *Node       `json:"rval" stype:"assign return"`
	Immediate   interface{} `json:"ival" stype:"literal"`
}

func IsShortNode(node *Node) bool {
	if node == nil {
		return true
	}
	switch node.Type {
	case "literal", "ref":
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
		fmt.Fprintf(f, `{"type": "ref", "refn": %d`, node.ID)
	default:
		fmt.Fprintf(f, "{\"type\":\"%s\"\n", node.Type)
		switch node.Type {
		case "var":
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
