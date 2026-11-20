# read memory of the running test game:  python tools/mem.py ADDR LEN
import ctypes, sys, os
k = ctypes.windll.kernel32
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
pid = int(open(os.path.join(ROOT, 'work', 'pid.txt')).read())
def read(a, n):
    h = k.OpenProcess(0x10, 0, pid); b = ctypes.create_string_buffer(n); r = ctypes.c_size_t()
    k.ReadProcessMemory(h, ctypes.c_void_p(a), b, n, ctypes.byref(r)); k.CloseHandle(h)
    return b.raw[:r.value]
if __name__ == '__main__':
    a = int(sys.argv[1], 16); n = int(sys.argv[2]) if len(sys.argv) > 2 else 32
    print(read(a, n).hex(' '))
