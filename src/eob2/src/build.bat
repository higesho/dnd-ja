@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0"
set O=..\build\release
set OUT=..\build\winmm.dll
set DEF=
set EXTRA=
if "%1"=="test" (
  set O=..\build\test
  set OUT=..\build\test\winmm.dll
  set DEF=/DDEVTEST
  set EXTRA=dev.c
)
if not exist ..\build mkdir ..\build
if not exist %O% mkdir %O%
rc /nologo /fo %O%\eob2_jp.res eob2_jp.rc || exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 /LD /D_CRT_SECURE_NO_WARNINGS %DEF% /Fo%O%\ proxy.c eob2_jp.c %EXTRA% %O%\eob2_jp.res /Fe%OUT% /link /DEF:winmm.def kernel32.lib user32.lib || exit /b 1
