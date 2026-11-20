@echo off
rem 日本語化ローダー wsock32.dll をビルドする（32 ビット）
setlocal
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cl /nologo /O2 /MT /W3 /LD /utf-8 ja_loader.c /link /DEF:wsock32.def /OUT:wsock32.dll /NOLOGO kernel32.lib || exit /b 1
del /q *.obj *.exp *.lib 2>nul
echo built wsock32.dll
