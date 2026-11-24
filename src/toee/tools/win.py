# Drive the test game (never the foreground window; the window is found by its title "TJP TEST" and pid).
#   python tools/win.py start [ENV=VAL ..]   copy build/test/DWrite.dll (test build) into the game, launch with TJP_TEST=1
#   python tools/win.py stop                 kill the game, remove the DLL
#   python tools/win.py shot NAME            back-buffer capture -> capture/NAME.png
#   python tools/win.py click X Y [right]    / move X Y / key VK.. / type TEXT / wait MS
import sys, os, time, mmap, struct, subprocess, shutil, ctypes, ctypes.wintypes as W
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.abspath(os.path.join(ROOT, '..'))
PIDF = os.path.join(ROOT, 'work', 'pid.txt')
u = ctypes.windll.user32
VK = {'esc': 0x1b, 'enter': 0x0d, 'space': 0x20, 'tab': 0x09, 'back': 0x08, 'up': 0x26, 'down': 0x28, 'left': 0x25, 'right': 0x27,
      'shift': 0x10, 'ctrl': 0x11, 'alt': 0x12, 'home': 0x24, 'end': 0x23, 'pgup': 0x21, 'pgdn': 0x22}
for i in range(1, 13): VK['f%d' % i] = 0x6f + i

def shm():
    m = mmap.mmap(-1, 16 + 260 + 4 + 256 + 128 + 256, tagname='TJP_TESTIO')
    if struct.unpack_from('<I', m, 0)[0] != 0x50454f54: raise SystemExit('game (test DLL) not running')
    return m

KOFS, KEYS, KEV, VKEYS = 276, 280, 536, 664

def dik(vk):
    sc = u.MapVirtualKeyW(vk, 0)
    if vk in (0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x2d, 0x2e): sc |= 0x80
    return sc

def dikey(m, code, down, vk=None):
    # DirectInput side: held state + one buffered event; GetKeyboardState side: vk held
    m[KEYS + code] = 0x80 if down else 0
    if vk is not None: m[VKEYS + vk] = 0x80 if down else 0
    n = struct.unpack_from('<i', m, KOFS)[0]
    struct.pack_into('<H', m, KEV + 2 * (n % 64), code | (0x100 if down else 0))
    struct.pack_into('<i', m, KOFS, n + 1)

def hwnd():
    pid = int(open(PIDF).read()); hw = []
    @ctypes.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
    def cb(h, l):
        p = W.DWORD(); u.GetWindowThreadProcessId(h, ctypes.byref(p))
        b = ctypes.create_unicode_buffer(64); u.GetWindowTextW(h, b, 64)
        if p.value == pid and b.value == 'TJP TEST': hw.append(h)
        return True
    u.EnumWindows(cb, 0)
    if not hw: raise SystemExit('game window not found')
    return hw[0]

def shot(name):
    from PIL import Image
    m = shm()
    os.makedirs(os.path.join(ROOT, 'capture'), exist_ok=True)
    bmp = os.path.join(ROOT, 'work', 'shot.bmp')
    if os.path.exists(bmp): os.remove(bmp)
    p = bmp.encode('mbcs') + b'\0'; m[16:16 + len(p)] = p
    req = struct.unpack_from('<i', m, 4)[0] + 1; struct.pack_into('<i', m, 4, req)
    t = time.time()
    while struct.unpack_from('<i', m, 8)[0] != req:
        if time.time() - t > 15: raise SystemExit('shot timeout')
        time.sleep(0.05)
    out = os.path.join(ROOT, 'capture', name + '.png')
    Image.open(bmp).convert('RGB').save(out); print(out)

def lp(x, y): return (int(y) << 16) | (int(x) & 0xffff)

def main():
    a = sys.argv[1:]
    c = a[0]
    if c == 'start':
        shutil.copy(os.path.join(ROOT, 'build', 'test', 'DWrite.dll'), os.path.join(GAME, 'DWrite.dll'))   # 試験版（src/build.bat test）
        env = dict(os.environ); env['TJP_TEST'] = '1'; env['TJP_LOG'] = os.path.join(ROOT, 'work', 'tjp.log')
        args = []
        for x in a[1:]:
            if '=' in x: k, v = x.split('=', 1); env[k] = v
            else: args.append(x)
        p = subprocess.Popen([os.path.join(GAME, 'temple.exe')] + args, cwd=GAME, env=env)
        open(PIDF, 'w').write(str(p.pid)); print('pid', p.pid)
    elif c == 'stop':
        try: subprocess.run(['taskkill', '/PID', open(PIDF).read().strip(), '/F'], capture_output=True)
        except Exception as e: print(e)
        time.sleep(1.5)
        try: os.remove(os.path.join(GAME, 'DWrite.dll'))
        except OSError as e: print(e)
    elif c == 'shot': shot(a[1])
    elif c == 'frame': print(struct.unpack_from('<i', shm(), 12)[0])
    elif c in ('click', 'move'):
        h = hwnd(); x, y = a[1], a[2]
        u.PostMessageW(h, 0x200, 0, lp(x, y)); time.sleep(0.15)
        if c == 'click':
            dn, up, mk = (0x204, 0x205, 2) if len(a) > 3 and a[3] == 'right' else (0x201, 0x202, 1)
            u.PostMessageW(h, dn, mk, lp(x, y)); time.sleep(0.1); u.PostMessageW(h, up, 0, lp(x, y)); time.sleep(0.3)
    elif c == 'dclick':
        h = hwnd(); x, y = a[1], a[2]
        u.PostMessageW(h, 0x200, 0, lp(x, y)); time.sleep(0.1)
        u.PostMessageW(h, 0x201, 1, lp(x, y)); time.sleep(0.05); u.PostMessageW(h, 0x202, 0, lp(x, y)); time.sleep(0.05)
        u.PostMessageW(h, 0x203, 1, lp(x, y)); time.sleep(0.05); u.PostMessageW(h, 0x202, 0, lp(x, y)); time.sleep(0.3)
    elif c == 'drag':
        h = hwnd(); x0, y0, x1, y1 = map(int, a[1:5])
        u.PostMessageW(h, 0x200, 0, lp(x0, y0)); time.sleep(0.15)
        u.PostMessageW(h, 0x201, 1, lp(x0, y0)); time.sleep(0.2)
        for i in range(1, 11):
            u.PostMessageW(h, 0x200, 1, lp(x0 + (x1 - x0) * i // 10, y0 + (y1 - y0) * i // 10)); time.sleep(0.04)
        time.sleep(0.15); u.PostMessageW(h, 0x202, 0, lp(x1, y1)); time.sleep(0.3)
    elif c == 'key':
        h = hwnd()
        for k in a[1:]:
            vk = VK.get(k.lower()) or (ord(k.upper()) if len(k) == 1 else int(k, 0))
            sc = u.MapVirtualKeyW(vk, 0)
            m = shm(); dikey(m, dik(vk), True, vk)
            u.PostMessageW(h, 0x100, vk, 1 | (sc << 16)); time.sleep(0.08)
            if len(k) == 1: u.PostMessageW(h, 0x102, ord(k), 1 | (sc << 16)); time.sleep(0.05)
            u.PostMessageW(h, 0x101, vk, 0xc0000001 | (sc << 16)); dikey(m, dik(vk), False, vk); time.sleep(0.12)
    elif c == 'hold':   # hold KEY MS: key down (with auto-repeat) for MS milliseconds
        h = hwnd(); k = a[1]
        vk = VK.get(k.lower()) or (ord(k.upper()) if len(k) == 1 else int(k, 0))
        sc = u.MapVirtualKeyW(vk, 0)
        t_end = time.time() + int(a[2]) / 1000
        first = True
        m = shm(); dikey(m, dik(vk), True, vk)
        while time.time() < t_end:
            u.PostMessageW(h, 0x100, vk, 1 | (sc << 16) | (0 if first else 0x40000000)); first = False
            time.sleep(0.03)
        u.PostMessageW(h, 0x101, vk, 0xc0000001 | (sc << 16)); dikey(m, dik(vk), False, vk)
    elif c == 'type':
        h = hwnd()
        for ch in ' '.join(a[1:]):
            u.PostMessageW(h, 0x102, ord(ch), 1); time.sleep(0.06)
    elif c == 'wait': time.sleep(int(a[1]) / 1000)

if __name__ == '__main__': main()
