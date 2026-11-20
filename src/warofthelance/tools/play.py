# start the game and press SPACE through the turn, capturing screens  (python tools/play.py N prefix)
import sys, time
sys.path.insert(0, 'tools')
import win
n = int(sys.argv[1]); pre = sys.argv[2]
win.start('jp'); time.sleep(7)
h, t = win.find_window(); u = win.u32
u.PostMessageW(h, 0x100, 0x10, 1 | (0x2a << 16)); time.sleep(0.1); win.post_key(h, 'e', 0.2)
u.PostMessageW(h, 0x101, 0x10, 1 | (0x2a << 16) | (3 << 30)); time.sleep(4)
for k in ['space', 'space', 'enter', 'space', 'space']:
    win.post_key(h, k); time.sleep(5)
for i in range(n):
    win.shot('%s%02d' % (pre, i)); win.post_key(h, 'space'); time.sleep(6)
from PIL import Image
ims = [Image.open('capture/%s%02d.png' % (pre, i)).resize((320, 200)) for i in range(n)]
c = Image.new('RGB', (1280, 200 * ((n + 3) // 4)))
for k, im in enumerate(ims): c.paste(im, ((k % 4) * 320, (k // 4) * 200))
c.save('capture/tt.png')
