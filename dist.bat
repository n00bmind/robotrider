@echo off
build.py --debug -v
cp bin\launcher.exe dist\launcher.debug.exe
cp bin\robotrider.dll dist\robotrider.debug.dll
build.py --release -v
cp bin\launcher.exe dist\launcher.exe
cp bin\robotrider.dll dist\robotrider.dll

