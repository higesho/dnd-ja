@echo off
rem 引数なし = 配布版 -> ..\build\dinput8.dll
rem build.bat test = 試験版、配布しない (/DDSJP_TEST + testio.c) -> ..\build\test\dinput8.dll
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0"
set O=..\build\release
set OUT=..\build\dinput8.dll
set DEF=
set EXTRA=
if "%1"=="test" (
  set O=..\build\test
  set OUT=..\build\test\dinput8.dll
  set DEF=/DDSJP_TEST
  set EXTRA=testio.c
)
if not exist ..\build mkdir ..\build
if not exist %O% mkdir %O%
rc /nologo /fo %O%\dsjp.res dsjp.rc || exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 /LD /D_CRT_SECURE_NO_WARNINGS %DEF% /Fo%O%\ proxy.c dsjp.c %EXTRA% arch.c explode.c text.c font.c wrap.c %O%\dsjp.res /Fe%OUT% /link /DEF:dinput8.def kernel32.lib user32.lib gdi32.lib || exit /b 1
