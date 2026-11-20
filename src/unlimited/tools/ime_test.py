# テスト用: 日本語の入力欄（DLL が F12 で出す Windows の入力欄）に文を入れて OK を押す。前面には出さない。
#   python -X utf8 tools/ime_test.py "入れたい文" [shot 名]
import sys, os, time, ctypes, ctypes.wintypes as W
sys.path.insert(0, os.path.dirname(__file__))
import win
u32 = ctypes.windll.user32
u32.SendMessageW.argtypes = [W.HWND, W.UINT, W.WPARAM, ctypes.c_wchar_p]
u32.FindWindowExW.restype = W.HWND
u32.FindWindowExW.argtypes = [W.HWND, W.HWND, ctypes.c_wchar_p, ctypes.c_wchar_p]

def find_dialog(pid, timeout=8):
    t0 = time.time()
    while time.time() - t0 < timeout:
        found = []
        def cb(h, l):
            p = W.DWORD()
            u32.GetWindowThreadProcessId(h, ctypes.byref(p))
            if p.value == pid:
                n = u32.GetWindowTextLengthW(h)
                b = ctypes.create_unicode_buffer(n + 1)
                u32.GetWindowTextW(h, b, n + 1)
                if '日本語入力' in b.value:
                    found.append(h)
            return True
        u32.EnumWindows(win.WNDENUMPROC(cb), 0)
        if found:
            return found[0]
        time.sleep(0.2)
    raise SystemExit('input box not found')

def shot_hwnd(h, name):
    from PIL import Image
    r = W.RECT(); u32.GetWindowRect(h, ctypes.byref(r))
    w, hh = r.right - r.left, r.bottom - r.top
    g32 = ctypes.windll.gdi32
    hdc = u32.GetWindowDC(h); mdc = g32.CreateCompatibleDC(hdc); bmp = g32.CreateCompatibleBitmap(hdc, w, hh)
    g32.SelectObject(mdc, bmp)
    u32.PrintWindow(h, mdc, 2)
    class BIH(ctypes.Structure):
        _fields_ = [('biSize', W.DWORD), ('biWidth', W.LONG), ('biHeight', W.LONG), ('biPlanes', W.WORD),
                    ('biBitCount', W.WORD), ('biCompression', W.DWORD), ('biSizeImage', W.DWORD),
                    ('biXPelsPerMeter', W.LONG), ('biYPelsPerMeter', W.LONG), ('biClrUsed', W.DWORD), ('biClrImportant', W.DWORD)]
    bi = BIH(ctypes.sizeof(BIH), w, -hh, 1, 32, 0, 0, 0, 0, 0, 0)
    buf = ctypes.create_string_buffer(w * hh * 4)
    g32.GetDIBits(mdc, bmp, 0, hh, buf, ctypes.byref(bi), 0)
    g32.DeleteObject(bmp); g32.DeleteDC(mdc); u32.ReleaseDC(h, hdc)
    out = os.path.join(win.ROOT, 'capture', name + '.png')
    Image.frombuffer('RGBA', (w, hh), buf.raw, 'raw', 'BGRA', 0, 1).convert('RGB').save(out)
    print(out)

def main():
    text = sys.argv[1]
    pid = int(open(win.PIDF).read())
    win.key(['f12'])
    h = find_dialog(pid)
    edit = u32.FindWindowExW(h, None, 'Edit', None)
    u32.SendMessageW(edit, 0x000C, 0, text)          # WM_SETTEXT
    time.sleep(0.5)
    if len(sys.argv) > 2:
        shot_hwnd(h, sys.argv[2])
    u32.PostMessageW(h, 0x0111, 1, 0)                # WM_COMMAND IDOK
    print('sent', len(text), 'chars')

if __name__ == '__main__':
    main()
