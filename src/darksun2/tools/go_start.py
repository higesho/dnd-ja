# テスト用: DLL を入れて起動し、タイトルで S（START GAME）→ ムービーを Esc で飛ばしながら 1 枚ずつ撮る
#   python -X utf8 tools/go_start.py [none|jp] [Esc の回数]
import os, sys, time
sys.path.insert(0, os.path.dirname(__file__))
import win
from go_menu import to_title, LOG

def main():
    v = sys.argv[1] if len(sys.argv) > 1 else 'jp'
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 8
    win.stop(); time.sleep(1)
    try: os.remove(LOG)
    except Exception: pass
    win.start(v); time.sleep(25)
    to_title()
    win.key(['s']); time.sleep(10)
    for i in range(n):
        win.shot('st%02d' % i)
        win.key(['esc']); time.sleep(5)
    for k in range(3):                      # Esc を押さずに待って撮る（会話の窓）
        time.sleep(4); win.shot('st%02d' % (n + k))
    print('done')

if __name__ == '__main__':
    main()
