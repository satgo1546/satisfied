package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"os"
	"os/exec"
	"testing"
)

func ReadNode(f io.Reader) *Node {
	n := new(Node)
	d := json.NewDecoder(f)
	d.Decode(n)
	return n
}

func DoFile(t *testing.T, filename string) {
	f, err := os.Open(filename)
	if err != nil {
		t.Error(err)
	}
	obj := ReadNode(f)
	var b bytes.Buffer
	WriteNode(&b, obj)
	str := make([]byte, b.Len())
	f.Seek(0, io.SeekStart)
	f.Read(str)
	f.Close()
	if !bytes.Equal(b.Bytes(), str) {
		fmt.Printf("%s\n", b.Bytes())
		t.Errorf("%s: differently formatted input and output", filename)
	}
	main := &Subroutine{
		Name: "main",
		Args: []any{},
	}
	obj.Retrocycle(nil)
	main.Code, main.Ret = obj.Compile(nil)
	fmt.Printf("%s\n", filename)
	RenumberInstructions(main.Code, 0)
	PrintInstructions(os.Stdout, main.Code, 1, '.')
	fmt.Println()
	DoProgram(t, main)
}

func DoProgram(t *testing.T, main *Subroutine) {
	MakeExe(main)
	out, err := exec.Command("./slzprog-output.exe").CombinedOutput()
	code := 0
	if exitErr, ok := err.(*exec.ExitError); ok {
		code = exitErr.ExitCode()
	} else {
		t.Errorf("bad executable: %v", err)
	}
	if code != 42 {
		t.Errorf("not 42 but %d", code)
	}
	fmt.Printf("%s", out)
}

func TestNestedIfInWhile(t *testing.T) {
	i0 := &Instruction{Opcode: OpConst, Const: 1}
	i1 := &Instruction{Opcode: OpConst, Const: 42}
	i2 := &Instruction{Opcode: OpWhile}
	i3 := &Instruction{Opcode: OpΦ, Arg1: i0}
	i4 := &Instruction{Opcode: OpAdd, Arg0: i3, Arg1: i0}
	i5 := &Instruction{Opcode: OpIfPositive, Arg0: i4}
	i6 := &Instruction{Opcode: OpICompare, Arg0: i4, Arg1: i1}
	i7 := &Instruction{Opcode: OpIfNegative, Arg0: i6}
	i0.Next = i1
	i1.Next = i2
	i2.Arg0 = i7
	i2.List0 = i3
	i3.Arg0 = i4
	i3.Next = i4
	i4.Next = i5
	i5.List0 = i6
	i6.Next = i7
	DoProgram(t, &Subroutine{Name: "main", Code: i0, Ret: i4})
}

func TestMe(t *testing.T) {
	for _, filename := range []string{
		"testdata/hello.json",
		"testdata/arith.json",
		"testdata/var.json",
		"testdata/if.json",
		"testdata/elsif.json",
		"testdata/ternary.json",
		"testdata/fib.json",
		"testdata/gcd.json",
	} {
		DoFile(t, filename)
	}
}
