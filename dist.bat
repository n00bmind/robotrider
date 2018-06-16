@echo off
call build
cp bin\rr.exe dist\rr_debug.exe
call build release
cp bin\rr.exe dist\rr_release.exe

