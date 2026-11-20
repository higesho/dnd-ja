#!/bin/sh
# テスト用: 保存データ A を読んで冒険を始め、コピープロテクトに答え、パランサスの「どこへ行くか？」まで進める
#   sh tools/go_palanthas.sh [jp|en]   (en = 訳なしの DLL で英語のまま)
cd "$(dirname "$0")/.."
L="$LOCALAPPDATA/DarkQueen_JP/log.txt"
python tools/win.py stop; sleep 1; rm -f "$L"
python tools/win.py start ${1:-jp} >/dev/null; sleep 10
for i in $(seq 1 25); do grep -q "1992" "$L" 2>/dev/null && break; python tools/win.py key esc >/dev/null; sleep 3; done
sleep 2
for c in p l t a; do python tools/win.py key $c >/dev/null; sleep 3; done
sleep 2; python tools/win.py key b >/dev/null; sleep 8
a=$(grep -o "copy protection answer [A-Za-z'-]*" "$L" | tail -1 | awk '{print $4}')
echo "answer: $a"
python tools/win.py key "text:$a" enter >/dev/null; sleep 8
for i in 1 2 3 4; do python tools/win.py key enter >/dev/null; sleep 3; done
python tools/win.py shot palanthas_${1:-jp}
