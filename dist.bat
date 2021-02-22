@echo off
build.py --dev
rm -rf dist
mkdir dist
cp bin\launcher.exe dist\launcher.exe
cp bin\robotrider.dll dist\robotrider.dll

