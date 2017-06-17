@echo off

set RR="."
set RRSRC=src
set RRBIN=bin

set DEBUGFLAGS=-DDEBUG=1 -Z7
set DISABLEFLAGS=-wd4201 -wd4100 -wd4189
set LIBS=user32.lib gdi32.lib winmm.lib

if not exist %RRBIN% mkdir %RRBIN%
pushd %RRBIN%
cl -MT -nologo -FC -W4 -WX -Oi -GR- -EHa- %DISABLEFLAGS% %DEBUGFLAGS% ..\%RRSRC%\win32_platform.cpp /link -opt:ref -subsystem:windows,5.2 %LIBS%
popd
