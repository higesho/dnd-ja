# テスト用: 起動して闘技場の門（アナウンサーの最初の台詞）まで進める。 bash tools/go_gate.sh <撮る名前> [jp|gog|none]
cd "/l/SteamLibrary/steamapps/common/Dungeons & Dragons Dark Sun Series/games/Dark Sun-ENG/_jp/tools"
V=${2:-jp}
python -X utf8 win.py stop; sleep 1; rm -f "$LOCALAPPDATA/DarkSun_JP/log.txt"
python -X utf8 win.py start $V; sleep 28
python -X utf8 -c "import sys; sys.path.insert(0,'.'); from go_arena import to_title; to_title()"
python -X utf8 win.py key s; sleep 10
for i in 1 2 3 4 5 6; do python -X utf8 win.py click 160 100; sleep 6; done
# 魔術師とランペイジャーの試合が終わって台詞の窓（上の石の枠）が出るまで待つ
python -X utf8 - <<'PY'
import sys, os, time
sys.path.insert(0, '.')
import win
from PIL import Image
for i in range(90):
    win.shot('_w')
    im = Image.open(os.path.join('..', 'capture', '_w.png')).convert('RGB')
    r, g, b = im.getpixel((500, 60))
    if abs(r - g) < 25 and b > r and 90 < b < 190 and im.getpixel((60, 50))[0] > 120: break   # 灰色の枠と顔の絵
    time.sleep(4)
PY
sleep 2; python -X utf8 win.py shot $1 >/dev/null; echo done
