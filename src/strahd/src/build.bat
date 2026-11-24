@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0"
set O=..\build\release
set OUT=..\build\winmm.dll
set DEF=
if "%1"=="test" (
  set O=..\build\test
  set OUT=..\build\test\winmm.dll
  set DEF=/DRLJP_TEST
)
if not exist ..\build mkdir ..\build
if not exist %O% mkdir %O%
rc /nologo /fo %O%\rl_jp.res rl_jp.rc || exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 /LD /D_CRT_SECURE_NO_WARNINGS %DEF% /Fo%O%\ proxy.c rl_jp.c %O%\rl_jp.res /Fe%OUT% /link /DEF:winmm.def kernel32.lib user32.lib || exit /b 1
