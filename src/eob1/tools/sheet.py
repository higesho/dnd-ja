# Test: join the dialogue areas (lower 164 px) of capture/NAME.png ... into capture/sheet_OUT.png
#   python tools/sheet.py OUT NAME [NAME..]
import sys, os
from PIL import Image
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
names = sys.argv[2:]
s = Image.new('RGB', (640, 166 * len(names)), (255, 255, 0))
for i, n in enumerate(names):
    im = Image.open(os.path.join(ROOT, 'capture', n + '.png'))
    s.paste(im.crop((0, 236, 640, 400)), (0, 166 * i))
out = os.path.join(ROOT, 'capture', 'sheet_' + sys.argv[1] + '.png')
s.save(out)
print(out)
