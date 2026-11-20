# テスト用: 会話の窓の選択肢 1 行目（client 150,318）をクリックして撮る
import sys, os, time
sys.path.insert(0, os.path.dirname(__file__))
import win
h, t = win.find_window()
x, y = int(sys.argv[1]), int(sys.argv[2])
tx, ty = 150, int(sys.argv[3]) if len(sys.argv) > 3 else 318
while (x, y) != (tx, ty):
    x += max(-6, min(6, tx - x)); y += max(-6, min(6, ty - y)); win.u32.PostMessageW(h, 0x200, 0, (y << 16) | x); time.sleep(0.03)
time.sleep(0.6); win.shot('cc0')
lp = (ty << 16) | tx
win.u32.PostMessageW(h, 0x201, 1, lp); time.sleep(0.1); win.u32.PostMessageW(h, 0x202, 0, lp)
for k in range(1, 4):
    time.sleep(3); win.shot('cc%d' % k)
