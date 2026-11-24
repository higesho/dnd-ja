#!/bin/sh
# translations -> data -> fonts -> dinput8.dll
set -e
cd "$(dirname "$0")/.."
python -X utf8 tools/make_data.py
python -X utf8 tools/make_font.py
cmd //c "src\build.bat" 2>&1 | grep -iE "error|warning C" || true
# 試験版（tools/win.py が使う。配布しない）
cmd //c "src\build.bat test" 2>&1 | grep -iE "error|warning C" || true
ls -la build/dinput8.dll build/test/dinput8.dll
