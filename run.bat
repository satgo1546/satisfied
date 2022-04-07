@echo off
nasm something.asm -o something.bin
if errorlevel 1 goto :eof
go run .
if errorlevel 1 goto :eof
slzprog.exe
if errorlevel 1 goto :eof
slzprog-output.exe
if errorlevel 1 echo Process exited with return value %ERRORLEVEL%.
