@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0"
if not exist ..\build mkdir ..\build
rc /nologo /fo ..\build\tjp.res tjp.rc || exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /LD /D_CRT_SECURE_NO_WARNINGS /Fo..\build\ proxy.c tjp.c testio.c render.c jpdraw.cpp text.c ..\build\tjp.res /Fe..\build\DWrite.dll /link /BASE:0x6A000000 /DEF:DWrite.def kernel32.lib user32.lib gdi32.lib d3d11.lib dxguid.lib d2d1.lib shell32.lib || exit /b 1
