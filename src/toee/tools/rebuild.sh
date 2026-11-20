#!/bin/sh
set -e
cd "$(dirname "$0")/.."
python -X utf8 tools/merge.py
python -X utf8 tools/make_data.py
python -X utf8 tools/make_font.py
cmd //c "src\build.bat" 2>&1 | grep -iE "error|warning C" || true
ls -la build/DWrite.dll
