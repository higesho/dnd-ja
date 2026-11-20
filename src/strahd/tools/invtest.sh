# usage: RLJP_SKIP=... sh tools/invtest.sh NAME  -> loads game 1, opens inventory, shot capture/NAME.png
cd "$(dirname "$0")/.."
python tools/win.py stop; sleep 1
python -X utf8 tools/totitle.py continue >/dev/null
python -X utf8 tools/win.py click 56 16; sleep 10
python -X utf8 tools/win.py click 160 100; sleep 2
python -X utf8 tools/win.py dclick 27 178; sleep 3
python -X utf8 tools/win.py shot $1 >/dev/null
