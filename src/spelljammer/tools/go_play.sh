#!/bin/sh
# テスト: 起動してタイトルの「ゲーム開始」を押し、コピープロテクトの答え（DLL のログ）を入れてゲームに入る
cd "$(dirname "$0")/.."
LOG="$LOCALAPPDATA/SJ_JP/log.txt"
python tools/win.py stop >/dev/null 2>&1
rm -f "$LOG"
SJ_LOG=1 python tools/win.py start ${1:-} >/dev/null
sleep 23
python tools/win.py key esc
sleep 4
python tools/win.py click 165 134
sleep 6
ANS=$(grep "copy protection answer" "$LOG" | tail -1 | sed 's/.*answer: //')
echo "answer: $ANS"
python tools/win.py key "text:$ANS"
sleep 1
python tools/win.py key enter
sleep 2
python tools/win.py key enter
sleep 10
python tools/win.py shot play0
