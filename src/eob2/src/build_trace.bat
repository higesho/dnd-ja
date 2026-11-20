@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0"
if not exist ..\build\trace mkdir ..\build\trace
cl /nologo /O2 /MT /W3 /LD /D_CRT_SECURE_NO_WARNINGS /Fo..\build\trace\ proxy.c trace.c /Fe..\build\trace\winmm.dll /link /DEF:winmm.def kernel32.lib user32.lib || exit /b 1
