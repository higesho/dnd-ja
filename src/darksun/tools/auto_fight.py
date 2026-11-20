# テスト用: 戦闘中、画面に見える怪物（緑 / 暗い灰色のかたまり）をクリックし続ける。会話の文（D 行）が増えたら止まる
import sys, os, time
sys.path.insert(0, os.path.dirname(__file__))
import win
from PIL import Image
LOG = os.path.join(os.environ['LOCALAPPDATA'], 'DarkSun_JP', 'log.txt')
def dcount(): return sum(1 for l in open(LOG, encoding='latin1') if l.startswith('D '))
h, t = win.find_window()
pos = [320, 220]
def mv(tx, ty):
    x, y = pos
    while (x, y) != (tx, ty):
        x += max(-6, min(6, tx - x)); y += max(-6, min(6, ty - y))
        win.u32.PostMessageW(h, 0x200, 0, (y << 16) | x); time.sleep(0.02)
    pos[0], pos[1] = x, y
def lc():
    lp = (pos[1] << 16) | pos[0]
    win.u32.PostMessageW(h, 0x201, 1, lp); time.sleep(0.08); win.u32.PostMessageW(h, 0x202, 0, lp)
n0 = dcount()
for it in range(int(sys.argv[1]) if len(sys.argv) > 1 else 40):
    win.shot('_f')
    im = Image.open(os.path.join(os.path.dirname(__file__), '..', 'capture', '_f.png')).convert('RGB')
    g = []; k = []
    for y in range(110, 392, 3):
        for x in range(8, 632, 3):
            if x > 420 and y < 80: continue
            r, gg, b = im.getpixel((x, y))
            if gg > 110 and r < 80 and b < 90: g.append((x, y))
            elif r < 70 and gg < 70 and 40 < b < 90 and abs(r - gg) < 12 and b - r < 30 and r > 30: k.append((x, y))
    tgt = g if len(g) >= 6 else (k if len(k) >= 25 else None)
    if tgt:
        tx = sorted(p[0] for p in tgt)[len(tgt) // 2]; ty = sorted(p[1] for p in tgt)[len(tgt) // 2]
        mv(tx, ty); time.sleep(0.2); lc()
        print(it, 'click', tx, ty, len(g), len(k))
    else:
        print(it, 'no target')
    time.sleep(3.5)
    mv(320, 230)
    if dcount() > n0: print('dialog'); break
win.shot('fight_end')
