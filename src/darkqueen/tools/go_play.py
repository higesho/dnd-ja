# テスト用: DLL を入れて起動し、保存データ A を読み込んで冒険を始め、コピープロテクトに答えるところまで進める
#   python -X utf8 tools/go_play.py
import os, sys, time, re, subprocess
sys.path.insert(0, os.path.dirname(__file__))
import win
LOG = os.path.join(os.environ['LOCALAPPDATA'], 'DarkQueen_JP', 'log.txt')

def k(*names, wait=3):
    win.key(list(names)); time.sleep(wait)

def main():
    win.stop(); time.sleep(1)
    if os.path.exists(LOG): os.remove(LOG)
    win.start('jp'); time.sleep(10)
    k('esc', wait=6)
    for c in 'plta':
        k(c)
    time.sleep(3)
    k('b', wait=8)
    ans = re.findall(r'copy protection answer (\S+)', open(LOG, encoding='latin1').read())
    if ans:
        k('text:' + ans[-1], 'enter', wait=8)
        print('answered', ans[-1])
    else:
        print('no copy protection answer found')

if __name__ == '__main__':
    main()
