# テスト用: 起動 → 保存データ A を読み込み → 冒険を始める（ログを見ながら進める）
import subprocess, sys, time, os
HERE = os.path.dirname(os.path.abspath(__file__))
LOG = os.path.join(os.environ['LOCALAPPDATA'], 'Champions_JP', 'log.txt')
def w(*a): subprocess.call([sys.executable, os.path.join(HERE, 'win.py')] + list(a), cwd=os.path.join(HERE, '..'), stdout=subprocess.DEVNULL)
def log():
    try: return open(LOG, encoding='latin1').read()
    except Exception: return ''
def wait_for(text, timeout=60, start=0):
    t = time.time()
    while time.time() - t < timeout:
        i = log().find(text, start)
        if i >= 0: return i + len(text)
        time.sleep(0.5)
    raise SystemExit('timeout ' + text)
w('stop'); time.sleep(1)
try: os.remove(LOG)
except Exception: pass
w('start', 'jp')
time.sleep(14)
for i in range(4): w('key', 'enter'); time.sleep(3)
w('key', 'esc')
time.sleep(2); w('key', 'down', 'down'); time.sleep(0.3); w('key', 'enter')
time.sleep(2); w('key', 'enter')                       # A
time.sleep(5); w('key', 'down', 'down', 'down', 'down', 'down'); time.sleep(0.3); w('key', 'enter')   # 冒険を始める
time.sleep(6)
print('playing')
