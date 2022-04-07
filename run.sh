#!/bin/sh
set -e
nasm something.asm -o something.bin
go run .
chmod +x slzprog-output.exe
./slzprog-output.exe
errorlevel=$?
[ $errorlevel -ge 1 ] && echo Process exited with return value $errorlevel.
