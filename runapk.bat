@echo off
gcc -o slzapk.exe slzapk.c
if errorlevel 1 goto :eof
slzapk.exe
if errorlevel 1 goto :eof
Q:\Prgm\WinRAR\WinRAR.exe slzapk-output.apk
