# Drive the test DOSBox window by its own process / unique title (never the foreground window).
#   python tools/win.py start [trace|jp|none]   launch DOSBox (DLL variant copied as winmm.dll)
#   python tools/win.py shot NAME               PrintWindow capture -> capture/NAME.png
#   python tools/win.py key KEY [KEY..]         post keys (names: enter esc space up down left right f1.. a..z 0..9, or 'text:HELLO')
#   python tools/win.py click X Y [right]       click at game coords (320x200)
#   python tools/win.py dclick X Y              double-click (opens a character sheet from a portrait)
#   python tools/win.py title | stop
import sys, os, time, ctypes, ctypes.wintypes as W, subprocess, shutil, struct
from ctypes import windll
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.abspath(os.path.join(ROOT, '..'))
PIDF = os.path.join(ROOT, 'test', 'pid.txt')
u32 = windll.user32; g32 = windll.gdi32

WNDENUMPROC = ctypes.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)

def find_window(pid=None):
    if pid is None:
        pid = int(open(PIDF).read())
    found = []
    def cb(h, l):
        p = W.DWORD()
        u32.GetWindowThreadProcessId(h, ctypes.byref(p))
        if p.value == pid and u32.IsWindowVisible(h):
            n = u32.GetWindowTextLengthW(h)
            b = ctypes.create_unicode_buffer(n + 1)
            u32.GetWindowTextW(h, b, n + 1)
            if b.value.startswith('DOSBox'):
                found.append((h, b.value))
        return True
    u32.EnumWindows(WNDENUMPROC(cb), 0)
    if not found:
        raise SystemExit('DOSBox window of pid %d not found' % pid)
    return found[0]

def client_size(h):
    if u32.IsIconic(h):
        u32.ShowWindow(h, 4)      # SW_SHOWNOACTIVATE: restore without stealing focus
        time.sleep(0.5)
    r = W.RECT()
    u32.GetClientRect(h, ctypes.byref(r))
    return r.right, r.bottom

def shot(name):
    from PIL import Image
    h, t = find_window()
    w, hh = client_size(h)
    hdc = u32.GetDC(h)
    mdc = g32.CreateCompatibleDC(hdc)
    bmp = g32.CreateCompatibleBitmap(hdc, w, hh)
    g32.SelectObject(mdc, bmp)
    if not u32.PrintWindow(h, mdc, 3):   # PW_CLIENTONLY | PW_RENDERFULLCONTENT
        g32.BitBlt(mdc, 0, 0, w, hh, hdc, 0, 0, 0x00CC0020)
    class BIH(ctypes.Structure):
        _fields_ = [('biSize', W.DWORD), ('biWidth', W.LONG), ('biHeight', W.LONG), ('biPlanes', W.WORD),
                    ('biBitCount', W.WORD), ('biCompression', W.DWORD), ('biSizeImage', W.DWORD),
                    ('biXPelsPerMeter', W.LONG), ('biYPelsPerMeter', W.LONG), ('biClrUsed', W.DWORD), ('biClrImportant', W.DWORD)]
    bi = BIH(ctypes.sizeof(BIH), w, -hh, 1, 32, 0, 0, 0, 0, 0, 0)
    buf = ctypes.create_string_buffer(w * hh * 4)
    g32.GetDIBits(mdc, bmp, 0, hh, buf, ctypes.byref(bi), 0)
    g32.DeleteObject(bmp); g32.DeleteDC(mdc); u32.ReleaseDC(h, hdc)
    im = Image.frombuffer('RGBA', (w, hh), buf.raw, 'raw', 'BGRA', 0, 1).convert('RGB')
    out = os.path.join(ROOT, 'capture', name + '.png')
    im.save(out)
    print(out, t)

VK = {'enter': (0x0d, 0x1c), 'esc': (0x1b, 0x01), 'space': (0x20, 0x39), 'up': (0x26, 0x48), 'down': (0x28, 0x50),
      'left': (0x25, 0x4b), 'right': (0x27, 0x4d), 'tab': (0x09, 0x0f), 'back': (0x08, 0x0e), 'pgup': (0x21, 0x49), 'pgdn': (0x22, 0x51),
      'home': (0x24, 0x47), 'end': (0x23, 0x4f)}
for i in range(1, 11):
    VK['f%d' % i] = (0x6f + i, 0x3a + i)
ROWS = ['1234567890', 'qwertyuiop', 'asdfghjkl', 'zxcvbnm']
SC0 = [0x02, 0x10, 0x1e, 0x2c]
for r, s0 in zip(ROWS, SC0):
    for k, ch in enumerate(r):
        VK[ch] = (ord(ch.upper()), s0 + k)

def post_key(h, name, hold=0.05):
    vk, sc = VK[name]
    ext = 1 << 24 if name in ('up', 'down', 'left', 'right', 'pgup', 'pgdn', 'home', 'end') else 0
    u32.PostMessageW(h, 0x0100, vk, 1 | (sc << 16) | ext)
    time.sleep(hold)
    u32.PostMessageW(h, 0x0101, vk, 1 | (sc << 16) | ext | (3 << 30))
    time.sleep(0.08)

def key(names):
    h, t = find_window()
    for n in names:
        if n.startswith('text:'):
            for ch in n[5:]:
                if ch == ' ':
                    post_key(h, 'space')
                else:
                    post_key(h, ch.lower())
        elif n.startswith('wait:'):
            time.sleep(float(n[5:]))
        else:
            post_key(h, n.lower())

def click(x, y, right=False, double=False):
    # DOSBox はマウスを相対移動で読む: いったん左上の隅へ押し付けてから目的地まで動かす
    h, t = find_window()
    w, hh = client_size(h)
    for k in range(6):
        u32.PostMessageW(h, 0x0200, 0, ((hh - 1) << 16) | (w - 1)); time.sleep(0.03)
        u32.PostMessageW(h, 0x0200, 0, 0); time.sleep(0.03)
    cx, cy = int(x * w / 320), int(y * hh / 200)
    # small steps (no mouse-driver acceleration), x first then y (tiny per-axis moves get lost)
    px = 0
    while px < cx:
        px = min(cx, px + 10); u32.PostMessageW(h, 0x0200, 0, px); time.sleep(0.03)
    py = 0
    while py < cy:
        py = min(cy, py + 10); u32.PostMessageW(h, 0x0200, 0, (py << 16) | cx); time.sleep(0.03)
    lp = (cy << 16) | cx
    time.sleep(0.15)
    down, up, mk = (0x0204, 0x0205, 2) if right else (0x0201, 0x0202, 1)
    for k in range(2 if double else 1):
        u32.PostMessageW(h, down, mk, lp); time.sleep(0.06 if double else 0.08)
        u32.PostMessageW(h, up, 0, lp); time.sleep(0.08)

def start(variant):
    dst = os.path.join(GAME, 'DOSBOX', 'winmm.dll')
    if os.path.exists(dst):
        os.remove(dst)
    if variant == 'none':
        pass
    elif variant == 'trace':
        shutil.copy(os.path.join(ROOT, 'build', 'trace', 'winmm.dll'), dst)
    elif variant == 'jp':   # 試験版（src\build.bat test で作る build\test\winmm.dll）
        shutil.copy(os.path.join(ROOT, 'build', 'test', 'winmm.dll'), dst)
    p = subprocess.Popen([os.path.join(GAME, 'DOSBOX', 'DOSBox.exe'), '-conf', '_jp\\test\\test.conf', '-noconsole'], cwd=GAME)
    open(PIDF, 'w').write(str(p.pid))
    for i in range(50):
        time.sleep(0.2)
        try:
            h, t = find_window(p.pid)
            print('pid', p.pid, 'window', t)
            return
        except SystemExit:
            pass
    print('pid', p.pid, 'window not found yet')

def stop():
    try:
        pid = int(open(PIDF).read())
        subprocess.call(['taskkill', '/PID', str(pid), '/F'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except Exception as e:
        print(e)

if __name__ == '__main__':
    c = sys.argv[1]
    if c == 'start': start(sys.argv[2] if len(sys.argv) > 2 else 'none')
    elif c == 'shot': shot(sys.argv[2])
    elif c == 'key': key(sys.argv[2:])
    elif c == 'click': click(int(sys.argv[2]), int(sys.argv[3]), 'right' in sys.argv[4:])
    elif c == 'dclick': click(int(sys.argv[2]), int(sys.argv[3]), double=True)
    elif c == 'title': print(find_window())
    elif c == 'stop': stop()
