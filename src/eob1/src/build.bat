@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0"
if not exist ..\build mkdir ..\build
rc /nologo /fo ..\build\eob_jp.res eob_jp.rc || exit /b 1
cl /nologo /O2 /MT /W3 /utf-8 /LD /D_CRT_SECURE_NO_WARNINGS /Fo..\build\ proxy.c eob_jp.c dev.c ..\build\eob_jp.res /Fe..\build\winmm.dll /link /DEF:winmm.def kernel32.lib user32.lib || exit /b 1
