package main

import (
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
	bytes, err := os.ReadFile("gcd.json")
	if err != nil {
		t.Error(err)
	}
	var obj *Node
	json.Unmarshal(bytes, &obj)
	t.Logf("%+v\n", obj)
	WriteNode(os.Stdout, obj)
}
