# capture a window by its title with PrintWindow (PW_RENDERFULLCONTENT) -> capture/NAME.png
import ctypes, ctypes.wintypes as W, sys, os
from PIL import Image
u = ctypes.windll.user32; g = ctypes.windll.gdi32
title = sys.argv[2] if len(sys.argv) > 2 else 'DSJP TEST'
h = u.FindWindowW(None, title)
if not h: raise SystemExit('window not found')
r = W.RECT(); u.GetClientRect(h, ctypes.byref(r))
w, hh = r.right, r.bottom
dc = u.GetDC(h); mdc = g.CreateCompatibleDC(dc); bmp = g.CreateCompatibleBitmap(dc, w, hh)
g.SelectObject(mdc, bmp)
ok = u.PrintWindow(h, mdc, 3)
class BIH(ctypes.Structure):
    _fields_ = [('biSize', W.DWORD), ('biWidth', W.LONG), ('biHeight', W.LONG), ('biPlanes', W.WORD), ('biBitCount', W.WORD),
                ('biCompression', W.DWORD), ('biSizeImage', W.DWORD), ('a', W.LONG), ('b', W.LONG), ('c', W.DWORD), ('d', W.DWORD)]
bi = BIH(); bi.biSize = 40; bi.biWidth = w; bi.biHeight = -hh; bi.biPlanes = 1; bi.biBitCount = 32
buf = ctypes.create_string_buffer(w * hh * 4)
g.GetDIBits(mdc, bmp, 0, hh, buf, ctypes.byref(bi), 0)
im = Image.frombuffer('RGBA', (w, hh), buf.raw, 'raw', 'BGRA', 0, 1).convert('RGB')
out = os.path.join(os.path.dirname(__file__), '..', 'capture', sys.argv[1] + '.png')
im.save(out); print(out, ok, w, hh)
