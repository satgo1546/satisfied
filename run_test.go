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
		main.Code, main.Ret = CompileNode(obj, nil, nil)
		fmt.Printf("%s\n", filename)
		RenumberInstructions(main.Code, 0)
		PrintInstructions(os.Stdout, main.Code, 1, '.')
		fmt.Println()
		MakeExe(main)
		out, err := exec.Command("./slzprog-output.exe").CombinedOutput()
		code := 0
		if exitErr, ok := err.(*exec.ExitError); ok {
			code = exitErr.ExitCode()
		} else {
			t.Errorf("bad executable in %s: %v", filename, err)
		}
		if code != 42 {
			t.Errorf("not 42 but %d in %s", code, filename)
		}
		fmt.Printf("%s", out)
	}
}
