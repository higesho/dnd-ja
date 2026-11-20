# wait until the main menu is on screen (menu box pixel), max ~6 minutes
import sys, time, os
sys.argv = ['x']
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import win
from PIL import Image
for i in range(40):
    win.shot('_wait')
    im = Image.open(os.path.join(win.ROOT, 'capture', '_wait.png'))
    if im.getpixel((300, 350)) == (81, 32, 81) and im.getpixel((60, 300)) == (174, 239, 251):
        print('menu after', i); break
    time.sleep(9)
