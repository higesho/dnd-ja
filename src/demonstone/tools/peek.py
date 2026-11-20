# Read dwords from the running test game: python tools/peek.py ADDR [ADDR..]  (prefix * to dereference)
import sys, os, ctypes, struct
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
pid = int(open(os.path.join(ROOT, 'work', 'pid.txt')).read())
k = ctypes.windll.kernel32
h = k.OpenProcess(0x0410, False, pid)
def rd(a, n=4):
    b = ctypes.create_string_buffer(n); got = ctypes.c_size_t()
    if not k.ReadProcessMemory(h, ctypes.c_void_p(a), b, n, ctypes.byref(got)): return None
    return b.raw
for s in sys.argv[1:]:
    deref = s.startswith('*'); a = int(s.lstrip('*'), 16)
    v = rd(a); v = struct.unpack('<I', v)[0] if v else None
    if deref and v is not None:
        w = rd(v); print('%s -> %08x -> %s' % (s, v, ('%08x (%d)' % (struct.unpack('<I', w)[0],) * 2) if w else '?'))
    else: print('%s = %s' % (s, '%08x (%d)' % (v, v) if v is not None else '?'))
