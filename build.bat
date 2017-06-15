@echo off

set RR="."
set RRSRC=src
set RRBIN=bin

set DEBUGFLAGS=-DDEBUG=1

if not exist %RRBIN% mkdir %RRBIN%
pushd %RRBIN%
cl -FC -Zi %DEBUGFLAGS% ..\%RRSRC%\win32_platform.cpp user32.lib gdi32.lib
popd
