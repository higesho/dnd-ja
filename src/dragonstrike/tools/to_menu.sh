#!/bin/sh
# テスト: 起動して、タイトルに著作権の行が出たら Enter（ロゴは Enter で飛ばす）
cd "$(dirname "$0")/.."
python tools/win.py stop >/dev/null 2>&1
rm -f "$LOCALAPPDATA/DS_JP/log.txt"
python tools/win.py start ${1:-} >/dev/null
sleep 9
python tools/win.py key enter wait:1 enter wait:1 enter wait:1 enter >/dev/null
for i in $(seq 1 30); do
  sleep 0.7
  python tools/win.py shot w >/dev/null 2>&1 || continue
  n=$(python -c "
from PIL import Image
im=Image.open('capture/w.png').convert('RGB'); print(len(im.crop((0,350,640,400)).getcolors(100000)))")
  if [ "$n" -gt 20 ]; then python tools/win.py key enter >/dev/null; echo "enter at $i"; break; fi
done
sleep 3
python tools/win.py click 300 190
sleep 1
python tools/win.py shot menu
