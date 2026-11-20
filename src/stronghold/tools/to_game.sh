#!/bin/sh
# 起動 → 紹介を飛ばす → 新規開始 → コピープロテクトに答える → 最初の街 → ゲーム開始
cd "$(dirname "$0")/.."
python tools/win.py key enter; sleep 30
for k in 1 2 3 4; do python tools/win.py key esc; sleep 3; done
python tools/win.py click 33 34; sleep 4
A=$(grep "copy answer" "$LOCALAPPDATA/SG_JP/log.txt" | tail -1 | sed 's/.*\[\(.*\)\]/\1/')
python tools/win.py key "text:${A#?}" enter; sleep 4
python tools/win.py click 33 34; sleep 5
python tools/win.py click 33 139; sleep 6
