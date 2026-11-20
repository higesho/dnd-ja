import sys, os, time
sys.path.insert(0, os.path.dirname(__file__))
import win
from PIL import Image
for i in range(80):
    win.shot('_w')
    im = Image.open(os.path.join(os.path.dirname(__file__), '..', 'capture', '_w.png')).convert('RGB')
    pts = [1 for y in range(0, 120, 4) for x in range(200, 440, 4) if (lambda p: p[1] > 120 and p[0] < 90 and p[2] < 90)(im.getpixel((x, y)))]
    if len(pts) > 20: print('gate'); break
    time.sleep(5)
