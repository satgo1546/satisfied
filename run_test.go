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

func TestMe(t *testing.T) {
	MakeExe()
	bytes, err := exec.Command("./slzprog-output.exe").CombinedOutput()
	code := 0
	if exitErr, ok := err.(*exec.ExitError); ok {
		code = exitErr.ExitCode()
	} else {
		t.Errorf("bad executable: %v", err)
	}
	if code != 42 {
		t.Errorf("not 42 but %d", code)
	}
	fmt.Printf("%s", bytes)
}

func ReadNode(f io.Reader) *Node {
	n := new(Node)
	d := json.NewDecoder(f)
	d.Decode(n)
	return n
}

func TestJSON(t *testing.T) {
	f, err := os.Open("gcd.json")
	if err != nil {
		t.Error(err)
	}
	defer f.Close()
	obj := ReadNode(f)
	var b bytes.Buffer
	WriteNode(&b, obj)
	str := make([]byte, b.Len())
	f.Seek(0, io.SeekStart)
	f.Read(str)
	if !bytes.Equal(b.Bytes(), str) {
		fmt.Printf("%s\n", b.Bytes())
		t.Errorf("differently formatted input and output")
	}
}
