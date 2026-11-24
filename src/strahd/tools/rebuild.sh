#!/bin/sh
# merge -> patch.bin -> winmm.dll ; usage: sh tools/rebuild.sh [log]
cd "$(dirname "$0")/.." || exit 1
if [ -f tools/merge.py ] && [ -d text/chunks ]; then python -X utf8 tools/merge.py || exit 1; fi
if [ "$1" = "log" ]; then RLJP_LOG=1 python -X utf8 tools/make_patch.py || exit 1; else python -X utf8 tools/make_patch.py || exit 1; fi
# 配布版 build/winmm.dll と試験版 build/test/winmm.dll（tools/win.py が使う）の両方を作る
cmd //c call "$(cygpath -w "$PWD/src/build.bat")" > build/build.log 2>&1
cmd //c call "$(cygpath -w "$PWD/src/build.bat")" test >> build/build.log 2>&1
if grep -q " error " build/build.log; then grep " error " build/build.log; exit 1; fi
ls -la build/winmm.dll build/test/winmm.dll
