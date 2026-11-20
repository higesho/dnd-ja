#!/bin/sh
# テスト用: チュートリアルのデザインを選び、保存データ A を読んで冒険を始める
#   sh tools/go_play.sh [jp|en]
cd "$(dirname "$0")/.."
L="$LOCALAPPDATA/UA_JP/log.txt"
k(){ python tools/win.py key "$@" >/dev/null; sleep ${W:-3}; }
python tools/win.py stop; sleep 1; rm -f "$L"
python tools/win.py start ${1:-jp} >/dev/null; sleep 10
for i in $(seq 1 20); do grep -qi "F [a-z]*[.]dsn" "$L" 2>/dev/null && break; k esc; done
sleep 1
k s; k s; W=5 k p; k l; W=5 k a; W=8 k b
python tools/win.py shot play
