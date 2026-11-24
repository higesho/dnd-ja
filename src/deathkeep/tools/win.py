# テスト用: DeathKeep をウインドウ（画面の外）で動かす。前面のウインドウではなく、自分が起動したプロセスのウインドウだけを扱う
#   python tools/win.py start [nojp]     ddraw.ini をテスト用に替え、build/test/winmm.dll（試験版）をゲームのフォルダに置いて DK.EXE を起動
#   python tools/win.py shot NAME        画面を capture/NAME.png に（DLL のテスト機能 DK_TEST=1 で、ゲームの画面を BMP に書かせる）
#   python tools/win.py key K [K..]      キー（DLL のテスト機能で GetAsyncKeyState / メッセージに入れる）
#   python tools/win.py click X Y [right]   ゲームの 320x200 の座標でクリック（画面写真 640x400 なら半分の値）
#   python tools/win.py stop             終了し、winmm.dll と ddraw.ini を元に戻す
import sys, os, time, struct, mmap, subprocess, shutil, ctypes, ctypes.wintypes as W
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.abspath(os.path.join(ROOT, '..'))
PIDF = os.path.join(ROOT, 'test', 'pid.txt')
u32 = ctypes.windll.user32
SHM = 'DK_TESTIO'
SHM_SIZE = 1344
# 共有メモリ: u32 magic, i32 req, i32 ack, i32 frame, char path[260] @16, u32 wr @300, u32 rd @304, cmd[64][4 i32] @320
VK = {'enter': 0x0d, 'esc': 0x1b, 'space': 0x20, 'up': 0x26, 'down': 0x28, 'left': 0x25, 'right': 0x27, 'tab': 0x09,
      'back': 0x08, 'pgup': 0x21, 'pgdn': 0x22, 'home': 0x24, 'end': 0x23, 'shift': 0x10, 'ctrl': 0x11}
for i in range(1, 13): VK['f%d' % i] = 0x6f + i
for c in '0123456789abcdefghijklmnopqrstuvwxyz': VK[c] = ord(c.upper())


def shm():
    m = mmap.mmap(-1, SHM_SIZE, tagname=SHM)
    if struct.unpack_from('<I', m, 0)[0] != 0x4b445453: raise SystemExit('test DLL not running')
    return m


def push(m, *cmds):
    for c in cmds:
        while True:
            wr, rd = struct.unpack_from('<II', m, 300)
            if wr - rd < 60: break
            time.sleep(0.02)
        c = list(c) + [0] * (4 - len(c))
        struct.pack_into('<4i', m, 320 + (wr % 64) * 16, *c)
        struct.pack_into('<I', m, 300, wr + 1)


def wait_done(m, t=20):
    s = time.time()
    while time.time() - s < t:
        wr, rd = struct.unpack_from('<II', m, 300)
        if wr == rd: return
        time.sleep(0.03)


def shot(name):
    from PIL import Image
    m = shm()
    wait_done(m)
    bmp = os.path.join(ROOT, 'test', 'shot.bmp')
    if os.path.exists(bmp): os.remove(bmp)
    p = bmp.encode('mbcs') + b'\0'
    m[16:16 + len(p)] = p
    req = struct.unpack_from('<i', m, 4)[0] + 1
    struct.pack_into('<i', m, 4, req)
    t = time.time()
    while struct.unpack_from('<i', m, 8)[0] != req:
        if time.time() - t > 15: raise SystemExit('shot timeout')
        time.sleep(0.05)
    im = Image.open(bmp).convert('RGB')
    if im.size == (320, 200): im = im.resize((640, 400), Image.NEAREST)
    out = os.path.join(ROOT, 'capture', name + '.png')
    im.save(out)
    print(out, im.size)


def key(names):
    m = shm()
    for n in names:
        if n.startswith('text:'):
            for ch in n[5:]:
                v = VK['space'] if ch == ' ' else VK[ch.lower()]
                push(m, (1, v, 60), (6, 80))
        elif n.startswith('wait:'):
            push(m, (6, int(float(n[5:]) * 1000)))
        else:
            push(m, (1, VK[n.lower()], 80), (6, 120))
    wait_done(m)


def click(x, y, right=False):
    m = shm()
    # cnc-ddraw のウインドウ 640x480 に 640x400 で表示（上下 40 の黒帯）
    x, y = x * 2 + 1, y * 2 + 41
    push(m, (2, x, y), (6, 150), (2, x, y), (6, 120), (3, 1 if right else 0, 100), (6, 200))
    wait_done(m)


def start(variant):
    ini = os.path.join(GAME, 'ddraw.ini')
    shutil.copy(os.path.join(ROOT, 'test', 'ddraw.ini'), ini)
    dst = os.path.join(GAME, 'winmm.dll')
    shutil.copy(os.path.join(ROOT, 'build', 'test', 'winmm.dll'), dst)   # 試験版（src/build.bat test）
    env = dict(os.environ); env['DK_TEST'] = '1'
    if variant == 'nojp': env['DK_NOJP'] = '1'
    p = subprocess.Popen([os.path.join(GAME, 'DK.EXE')], cwd=GAME, env=env)
    open(PIDF, 'w').write(str(p.pid))
    print('pid', p.pid)


def stop():
    try:
        pid = int(open(PIDF).read())
        subprocess.call(['taskkill', '/PID', str(pid), '/F'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except Exception as e:
        print(e)
    time.sleep(1)
    for f in ('winmm.dll',):
        p = os.path.join(GAME, f)
        if os.path.exists(p): os.remove(p)
    shutil.copy(os.path.join(ROOT, 'work', 'ddraw.ini.orig'), os.path.join(GAME, 'ddraw.ini'))


if __name__ == '__main__':
    c = sys.argv[1]
    if c == 'start': start(sys.argv[2] if len(sys.argv) > 2 else '')
    elif c == 'shot': shot(sys.argv[2])
    elif c == 'key': key(sys.argv[2:])
    elif c == 'click': click(int(sys.argv[2]), int(sys.argv[3]), 'right' in sys.argv[4:])
    elif c == 'stop': stop()
