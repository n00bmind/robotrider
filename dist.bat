@echo off
build.py --dev
mkdir dist
cp bin\launcher.exe dist\launcher.exe
cp bin\robotrider.dll dist\robotrider.dll

