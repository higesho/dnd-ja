# テスト用: 闘技場で画面を上へ流し、アナウンサー（緑の服）を「話す」カーソルでクリックする
import sys, os, time
sys.path.insert(0, os.path.dirname(__file__))
import win
from PIL import Image
h, t = win.find_window()
def mv(x, y): win.u32.PostMessageW(h, 0x200, 0, (y << 16) | x); time.sleep(0.04)
def btn(x, y, right=False):
    d, u, m = (0x204, 0x205, 2) if right else (0x201, 0x202, 1)
    lp = (y << 16) | x
    win.u32.PostMessageW(h, d, m, lp); time.sleep(0.08); win.u32.PostMessageW(h, u, 0, lp); time.sleep(0.6)
for k in range(25):
    mv(300 + (k & 1), 1); time.sleep(0.08)
mv(320, 200); time.sleep(0.5)
win.shot('ta0')
im = Image.open(os.path.join(os.path.dirname(__file__), '..', 'capture', 'ta0.png')).convert('RGB')
pts = [(x, y) for y in range(0, 300, 2) for x in range(0, 640, 2)
       if (lambda p: p[1] > 120 and p[0] < 90 and p[2] < 90)(im.getpixel((x, y)))]
if not pts: print('no announcer'); sys.exit(1)
tx = sum(p[0] for p in pts) // len(pts); ty = sum(p[1] for p in pts) // len(pts)
x, y = 320, 200
while (x, y) != (tx, ty):
    x += max(-6, min(6, tx - x)); y += max(-6, min(6, ty - y)); mv(x, y)
time.sleep(0.4)
mode = sys.argv[1] if len(sys.argv) > 1 else '2'
for i in range(int(mode)): btn(x, y, True)
btn(x, y)
time.sleep(4); win.shot('ta1')
print('clicked', tx, ty)
