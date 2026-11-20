# テスト用: DLL を入れて起動し、タイトルの「デモ」を流して戦闘の場面を撮る（capture/dsheet.png に並べる）
#   python -X utf8 tools/go_demo.py [待ち秒数=100] [枚数=12] [間隔=7]
import os, sys, time
sys.path.insert(0, os.path.dirname(__file__))
import win
from PIL import Image
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
LOG = os.path.join(os.environ['LOCALAPPDATA'], 'DarkQueen_JP', 'log.txt')

def main():
    wait = int(sys.argv[1]) if len(sys.argv) > 1 else 100
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 12
    step = int(sys.argv[3]) if len(sys.argv) > 3 else 7
    win.stop(); time.sleep(1)
    if os.path.exists(LOG): os.remove(LOG)
    win.start('jp'); time.sleep(10)
    win.key(['esc']); time.sleep(6)
    win.key(['d']); time.sleep(wait)
    ims = []
    for i in range(n):
        name = 'demo%02d' % i
        win.shot(name)
        ims.append(Image.open(os.path.join(ROOT, 'capture', name + '.png')).resize((320, 200)))
        time.sleep(step)
    cols = 3
    sheet = Image.new('RGB', (320 * cols, 200 * ((n + cols - 1) // cols)))
    for k, im in enumerate(ims):
        sheet.paste(im, ((k % cols) * 320, (k // cols) * 200))
    sheet.save(os.path.join(ROOT, 'capture', 'dsheet.png'))
    print('done')

if __name__ == '__main__':
    main()
