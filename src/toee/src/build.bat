@echo off
rem 引数なし = 配布版 -> ..\build\DWrite.dll
rem build.bat test = 試験版、配布しない (/DTJP_TEST + testio.c) -> ..\build\test\DWrite.dll
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0"
set O=..\build\release
set OUT=..\build\DWrite.dll
set DEF=
set EXTRA=
if "%1"=="test" (
  set O=..\build\test
  set OUT=..\build\test\DWrite.dll
  set DEF=/DTJP_TEST
  set EXTRA=testio.c
)
if not exist ..\build mkdir ..\build
if not exist %O% mkdir %O%
rc /nologo /fo %O%\tjp.res tjp.rc || exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 /EHsc /LD /D_CRT_SECURE_NO_WARNINGS %DEF% /Fo%O%\ proxy.c tjp.c %EXTRA% render.c jpdraw.cpp text.c %O%\tjp.res /Fe%OUT% /link /BASE:0x6A000000 /DEF:DWrite.def kernel32.lib user32.lib gdi32.lib d3d11.lib dxguid.lib d2d1.lib shell32.lib || exit /b 1
