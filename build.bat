@echo off
setlocal ENABLEDELAYEDEXPANSION

ctime -begin rr.time

set RR="."
set RRSRC=src
set RRBIN=bin

set COMPILER=cl
::set COMPILER=clang-cl

set LIBS=user32.lib gdi32.lib winmm.lib ole32.lib opengl32.lib shlwapi.lib
set WARNINGFLAGS=-wd4201 -wd4100 -wd4189 -wd4101 -wd4505 -wd4312
set COMMONFLAGS=-MTd -nologo -FC -W4 -WX -Oi -GR- -EHa- -D_HAS_EXCEPTIONS=0 -D_CRT_SECURE_NO_WARNINGS %WARNINGFLAGS%
set LINKERFLAGS=/opt:ref /incremental:no

set CLANGFLAGS=-fdiagnostics-absolute-paths -Wno-missing-braces -Wno-unused-variable -Wno-unused-function -Wno-missing-field-initializers
set COMPILERFLAGS=
if %COMPILER%==clang-cl (
    set COMPILERFLAGS=%CLANGFLAGS%
)

set DEBUGFLAGS=-DDEBUG=1 -Z7 -Od
set RELEASEFLAGS=-O2
set CFGFLAGS=%DEBUGFLAGS%
(echo ";r;rel;release;" | findstr /i ";%1;" 1>nul 2>nul) && (
    set CFGFLAGS=%RELEASEFLAGS%
)

if not exist %RRBIN% mkdir %RRBIN%
pushd %RRBIN%
del *.pdb >NUL 2>NUL
%COMPILER% %COMMONFLAGS% %CFGFLAGS% %COMPILERFLAGS% ..\%RRSRC%\robotrider.cpp -LD /link %LINKERFLAGS% /PDB:rr_dll_%random%.pdb
%COMPILER% %COMMONFLAGS% %CFGFLAGS% %COMPILERFLAGS% ..\%RRSRC%\win32_platform.cpp -Felauncher.exe /link %LINKERFLAGS% -subsystem:console,5.2 %LIBS%
popd

ctime -end rr.time

