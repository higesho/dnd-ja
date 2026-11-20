# start the jp build and get to the title menu, then click a menu item: python tools/totitle.py [continue|create]
import sys, time, os
item = sys.argv[1] if len(sys.argv) > 1 else 'continue'
sys.argv = ['x']
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import win
from PIL import Image
win.start(os.environ.get('VARIANT', 'jp'))
time.sleep(5)
for i in range(30):
    win.key(['esc'])
    time.sleep(2)
    win.shot('_wait')
    im = Image.open(os.path.join(win.ROOT, 'capture', '_wait.png'))
    if im.getpixel((300, 210)) == im.getpixel((300, 210)) and im.getpixel((20, 10)) != (0, 0, 0) and abs(im.getpixel((330, 240))[0] - 120) < 80 and im.getpixel((330, 240))[0] > im.getpixel((330, 240))[2]:
        break
time.sleep(2)
y = {'continue': 145, 'create': 154}[item]
h, t = win.find_window()
w, hh = win.client_size(h)
for yy in (y - 4, y - 2, y):
    win.u32.PostMessageW(h, 0x0200, 0, (int(yy * hh / 200) << 16) | int(160 * w / 320)); time.sleep(0.4)
win.click(160, y)
time.sleep(8)
win.shot('_menu')
