#!/bin/sh
# 起動から本編までを繰り返し、DOSBox が応答しなくなるかを数える（$1 = 回数、SG_NOJP を付ければ英語）
cd "$(dirname "$0")/.."
for i in $(seq 1 ${1:-3}); do
  python tools/win.py stop >/dev/null 2>&1; rm -f "$LOCALAPPDATA/SG_JP/log.txt"
  python tools/win.py start >/dev/null; sleep 8
  sh tools/to_game.sh >/dev/null 2>&1
  python tools/win.py click 250 138; sleep 4
  r=$(powershell -NoProfile -Command "(Get-Process -Id (Get-Content test/pid.txt)).Responding")
  f=$(python -c "import mmap,struct,time;m=mmap.mmap(-1,316,tagname='SG_TESTIO');a=struct.unpack_from('<i',m,12)[0];time.sleep(1.5);print(a,struct.unpack_from('<i',m,12)[0])")
  echo "run $i responding=$r frames=$f"
done
python tools/win.py stop >/dev/null 2>&1
