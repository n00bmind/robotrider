@echo off
call build
cp bin\launcher.exe dist\launcher.debug.exe
cp bin\robotrider.dll dist\robotrider.debug.dll
call build release
cp bin\launcher.exe dist\launcher.exe
cp bin\robotrider.dll dist\robotrider.dll

