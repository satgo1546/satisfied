package main

import (
	"bytes"
	"encoding/json"
	"fmt"
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

func TestJSON(t *testing.T) {
	contents, err := os.ReadFile("gcd.json")
	if err != nil {
		t.Error(err)
	}
	var obj *Node
	json.Unmarshal(contents, &obj)
	var b bytes.Buffer
	WriteNode(&b, obj)
	if !bytes.Equal(b.Bytes(), contents) {
		fmt.Printf("%s\n", b.Bytes())
		t.Errorf("differently formatted input and output")
	}
}
