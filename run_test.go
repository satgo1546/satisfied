package main

import (
	"fmt"
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
