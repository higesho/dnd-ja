#!/bin/sh
# テスト: ゲームに入り、ANADIA へスペルジャムして UMBERGUARD に入港する（go_play.sh のあと）
cd "$(dirname "$0")/.."
python tools/win.py click 275 12; sleep 3
python tools/win.py key n; sleep 3
python tools/win.py key n; sleep 3
python tools/win.py key x; sleep 3
for i in 1 2 3; do python tools/win.py click 243 107; sleep 1; done
sleep 15
python tools/win.py click 275 12; sleep 3
python tools/win.py key s; sleep 75
python tools/win.py shot orbit
python tools/win.py key u; sleep 8
python tools/win.py shot port0
