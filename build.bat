@echo off

set RR="R:"
set RRSRC=%RR%\src
set RRBIN=%RR%\bin

if not exist %RRBIN% mkdir %RRBIN%
pushd %RRBIN%
cl -Zi %RRSRC%\win32_platform.cpp user32.lib
popd
