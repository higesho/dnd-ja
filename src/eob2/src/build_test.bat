@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0"
if not exist ..\build\test mkdir ..\build\test
cl /nologo /utf-8 /O2 /MT /W3 /LD /DDEVTEST /D_CRT_SECURE_NO_WARNINGS /Fo..\build\test\ proxy.c eob2_jp.c dev.c ..\build\eob2_jp.res /Fe..\build\test\winmm.dll /link /DEF:winmm.def kernel32.lib user32.lib || exit /b 1
