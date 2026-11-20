#!/bin/sh
# テスト用: チュートリアルのデザインを開き、Dungeon 01 に文章イベント（TEXT STATEMENT）を置いて入力欄をクリックするところまで進める
#   [DOWN=13] sh tools/go_text.sh [jp|en]
cd "$(dirname "$0")/.."
L="$LOCALAPPDATA/UA_JP/log.txt"
k(){ python tools/win.py key "$@" >/dev/null; sleep ${W:-3}; }
python tools/win.py stop; sleep 1; rm -f "$L" "$LOCALAPPDATA/UA_JP/trace.txt"
python tools/win.py start ${1:-jp} >/dev/null; sleep 10
for i in $(seq 1 20); do grep -qi "F [a-z]*[.]dsn" "$L" 2>/dev/null && break; k esc; done
sleep 1
k s; k s; W=4 k e
# 何番目のモジュールを開くか（既定 4 = Dungeon 01。チュートリアルの保存データのパーティーは 13 = Dungeon 10 にいる）
for i in $(seq 1 ${DOWN:-4}); do python tools/win.py key down >/dev/null; done; sleep 1
W=6 k o
python tools/win.py hold 135 6 >/dev/null; sleep 1; python tools/win.py release 170 78 >/dev/null; sleep 3
k p
python tools/win.py key pgdn pgdn pgdn >/dev/null; sleep 1; python tools/win.py key up up >/dev/null; sleep 1
W=4 k s
python tools/win.py click 160 128 >/dev/null; sleep 2
python tools/win.py shot text_page
