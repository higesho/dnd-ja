# テスト用: DLL を入れて起動し、オープニングを飛ばして START GAME → 闘技場の最初の戦闘まで進め、画面を撮る
#   python -X utf8 tools/go_arena.py [none|jp]
import os, sys, time
sys.path.insert(0, os.path.dirname(__file__))
import win

def to_title(maxtries=14):
    """オープニングを Esc で飛ばし、タイトルのメニュー（石の枠）が出たら止める（タイトルで Esc を押すと終わってしまう）"""
    from PIL import Image
    for i in range(maxtries):
        win.shot('_probe')
        im = Image.open(os.path.join(os.path.dirname(__file__), '..', 'capture', '_probe.png'))
        r, g, b = im.getpixel((100, 250))[:3]
        if 55 <= r <= 85 and 55 <= g <= 85 and 80 <= b <= 110:
            return True
        win.key(['esc']); time.sleep(4)
    return False

LOG = os.path.join(os.environ['LOCALAPPDATA'], 'DarkSun2_JP', 'log.txt')

def main():
    v = sys.argv[1] if len(sys.argv) > 1 else 'jp'
    win.stop(); time.sleep(1)
    try: os.remove(LOG)
    except Exception: pass
    win.start(v); time.sleep(20)
    time.sleep(8)
    to_title()
    win.click(155, 75); time.sleep(10)          # START GAME
    for i in range(6):
        win.click(160, 100); time.sleep(6)
    win.shot('arena')
    print('done')

if __name__ == '__main__':
    main()
