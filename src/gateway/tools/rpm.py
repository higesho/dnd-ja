import ctypes, struct, sys
from ctypes import windll, wintypes as W
k32 = windll.kernel32
def open_proc(pid):
    return k32.OpenProcess(0x0410 | 0x0020 | 0x0008, False, pid)
def read(h, addr, n):
    buf = ctypes.create_string_buffer(n); got = ctypes.c_size_t()
    if not k32.ReadProcessMemory(h, ctypes.c_void_p(addr), buf, n, ctypes.byref(got)): return None
    return buf.raw[:got.value]

class Guest:
    def __init__(self, pid=None):
        import os
        if pid is None:
            pid = int(open(os.path.join(os.path.dirname(__file__), '..', 'test', 'pid.txt')).read())
        self.h = open_proc(pid)
        tl = struct.unpack('<256I', read(self.h, 0x74d6d0, 1024))
        import collections
        self.membase = collections.Counter(x for x in tl[:0xa0] if x).most_common(1)[0][0]
    def mem(self, lin, n):
        return read(self.h, self.membase + lin, n)
    def w(self, lin): return struct.unpack('<H', self.mem(lin, 2))[0]
    def regs(self):
        r = struct.unpack('<9I', read(self.h, 0x74d1e0, 36)); s = struct.unpack('<8H', read(self.h, 0x74d100, 16))
        return r, s
