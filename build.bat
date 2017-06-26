@echo off

set RR="."
set RRSRC=src
set RRBIN=bin

set COMMONFLAGS=-MT -nologo -FC -W4 -WX -Oi -GR- -EHa-
set LINKERFLAGS=/opt:ref /incremental:no
set DISABLEFLAGS=-wd4201 -wd4100 -wd4189 -wd4127 -wd4101
set DEBUGFLAGS=-DDEBUG=1 -Z7
set LIBS=user32.lib gdi32.lib winmm.lib ole32.lib

set hh=%time:~0,2%
if "%time:~0,1%"==" " set hh=0%hh:~1,1%
set suffix=%date:~-4,4%%date:~-10,2%%date:~-7,2%_%hh%%time:~3,2%%time:~6,2%

if not exist %RRBIN% mkdir %RRBIN%
pushd %RRBIN%
del *.pdb >NUL 2>NUL
cl %COMMONFLAGS% %DISABLEFLAGS% %DEBUGFLAGS% ..\%RRSRC%\robotrider.cpp -Fmrobotrider.map -LD /link %LINKERFLAGS% /PDB:rr_%suffix%.pdb /EXPORT:GameUpdateAndRender
cl %COMMONFLAGS% %DISABLEFLAGS% %DEBUGFLAGS% ..\%RRSRC%\win32_platform.cpp -Fmwin32_platform.map /link %LINKERFLAGS% -subsystem:windows,5.2 %LIBS%
popd
