@echo off

for %%f in (.\src\*.cpp) do (
    echo Compiling %%f...
    cl.exe /c /Fo.\bin\ /Od /DNON_UNITY_BUILD %%f
    if errorlevel 1 goto:EOF
)

