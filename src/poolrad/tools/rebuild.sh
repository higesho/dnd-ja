#!/bin/sh
# 訳をまとめて winmm.dll を作り直す（作業フォルダ _jp で実行）
set -e
cd "$(dirname "$0")/.."
python -X utf8 tools/make_patch.py
cmd //c "src\build.bat" | grep -iE "error|warning C" || true
ls -la build/winmm.dll
