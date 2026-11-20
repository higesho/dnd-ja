#!/bin/sh
# テスト: メニュー → 最初の任務 → コピープロテクト → 前置き → 地図 → 飛行
cd "$(dirname "$0")/.."
sh tools/to_menu.sh ${1:-} >/dev/null
python tools/win.py click 160 91
python tools/win.py key wait:4 text:abc enter wait:4 enter wait:3 enter wait:3 enter wait:3 enter wait:3 enter wait:5 >/dev/null
python tools/win.py shot flight
