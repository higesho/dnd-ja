@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0"
if not exist ..\build mkdir ..\build
rc /nologo /fo ..\build\dq_jp.res dq_jp.rc || exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 /LD /D_CRT_SECURE_NO_WARNINGS /Fo..\build\ proxy.c dq_jp.c testio.c ..\build\dq_jp.res /Fe..\build\winmm.dll /link /DEF:winmm.def kernel32.lib user32.lib gdi32.lib || exit /b 1
