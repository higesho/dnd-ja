# step helpers: python tools/step.py title | cont | inv   (each takes a shot step_*.png)
import sys, time, os
cmd = sys.argv[1]; sys.argv = ['x']
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import win
def move(x, y):
    h, t = win.find_window(); w, hh = win.client_size(h)
    for d in (4, 2, 0):
        win.u32.PostMessageW(h, 0x0200, 0, (int((y - d) * hh / 200) << 16) | int(x * w / 320)); time.sleep(0.3)
def at_title():
    from PIL import Image, ImageChops, ImageStat
    win.shot('_wait')
    a = Image.open(os.path.join(win.ROOT, 'capture', '_wait.png')).crop((150, 140, 490, 230)).convert('L')
    b = Image.open(os.path.join(win.ROOT, 'test', 'title_ref.png')).crop((150, 140, 490, 230)).convert('L')
    return ImageStat.Stat(ImageChops.difference(a, b)).mean[0] < 12
if cmd == 'title':
    for i in range(60):
        if at_title(): break
        if i % 2 == 0: win.key(['esc'])
        time.sleep(1.5)
    time.sleep(1)
elif cmd == 'cont':
    move(160, 145); time.sleep(0.5); win.shot('step_cont0'); win.click(160, 145); time.sleep(8)
elif cmd == 'create':
    move(160, 154); win.click(160, 154); time.sleep(14)
elif cmd == 'load1':
    win.click(56, 16); time.sleep(10); win.click(160, 100); time.sleep(2)
elif cmd == 'inv':
    win.click(27, 178, double=True); time.sleep(3)
win.shot('step_' + cmd)
if cmd == 'menubar':
    h, t = win.find_window(); w, hh = win.client_size(h)
    x = int(sys.argv[1]) if False else 160
    win.click(160, 150); time.sleep(1)
    for y in (60, 30, 10, 3, 0):
        win.u32.PostMessageW(h, 0x0200, 0, (int(y * hh / 200) << 16) | int(160 * w / 320)); time.sleep(0.3)
    time.sleep(1.5); win.shot('step_menubar')
