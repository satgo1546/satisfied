#!/bin/sh
set -e
nasm something.asm -o something.bin
[ -n "$1" ] && gcc -o slzprog.exe slzprog.c
./slzprog.exe
chmod +x slzprog-output.exe
./slzprog-output.exe
errorlevel=$?
[ $errorlevel -ge 1 ] && echo Process exited with return value $errorlevel.
