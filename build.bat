@echo off

set RR="W:"
set RRSRC=%RR%\src
set RRBIN=%RR%\bin

if not exist %RRBIN% mkdir %RRBIN%
pushd %RRBIN%
cl -FC -Zi %RRSRC%\win32_platform.cpp user32.lib gdi32.lib
popd
