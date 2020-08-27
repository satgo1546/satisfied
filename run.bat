@echo off
nasm something.asm -o something.bin
if errorlevel 1 goto :eof
if "%1"=="" goto :no_compile
gcc -o slzprog.exe slzprog.c
if errorlevel 1 goto :eof
:no_compile
slzprog.exe
if errorlevel 1 goto :eof
slzprog-output.exe
if errorlevel 1 echo Process exited with return value %ERRORLEVEL%.
