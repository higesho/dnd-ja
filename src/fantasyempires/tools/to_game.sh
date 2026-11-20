#!/bin/sh
# タイトル → キャンペーン → P1 = 作ったキャラクター（TESTER）, P2 = NPC → 開始 → 手番のカードを閉じる
cd "$(dirname "$0")/.."
python tools/win.py key esc; sleep 2; python tools/win.py key esc; sleep 3
python tools/win.py click 265 162; sleep 5
python tools/win.py click 61 106; sleep 1
python tools/win.py click 92 106; sleep 1
python tools/win.py click 288 160; sleep 18
python tools/win.py fclick 158 139; sleep 6
