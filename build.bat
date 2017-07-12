@echo off
setlocal ENABLEDELAYEDEXPANSION

set RR="."
set RRSRC=src
set RRBIN=bin

set LIBS=user32.lib gdi32.lib winmm.lib ole32.lib opengl32.lib
set COMMONFLAGS=-MTd -nologo -FC -W4 -WX -Oi -GR- -EHa-
set LINKERFLAGS=/opt:ref /incremental:no
set DISABLEFLAGS=-wd4201 -wd4100 -wd4189 -wd4127 -wd4101 -wd4505
set DEBUGFLAGS=-DDEBUG=1 -Z7 -Od
set RELEASEFLAGS=-O2

set CFGFLAGS=%DEBUGFLAGS%
(echo ";r;rel;release;" | findstr /i ";%1;" 1>nul 2>nul) && (
    set CFGFLAGS=%RELEASEFLAGS%
)

if not exist %RRBIN% mkdir %RRBIN%
pushd %RRBIN%
del *.pdb >NUL 2>NUL
cl %COMMONFLAGS% %DISABLEFLAGS% %CFGFLAGS% ..\%RRSRC%\robotrider.cpp -Fmrobotrider.map -LD /link %LINKERFLAGS% /PDB:rr_%random%.pdb
cl %COMMONFLAGS% %DISABLEFLAGS% %CFGFLAGS% ..\%RRSRC%\win32_platform.cpp -Fmwin32_platform.map /link %LINKERFLAGS% -subsystem:windows,5.2 %LIBS%
popd
