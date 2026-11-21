# Test helper: overwrite the level-1 "fallen rocks" script message (runs when the rubble at the start is clicked: win.py click 88 60) in guest memory with other script bytes.
#   python tools/poke.py find            show where the script is
#   python tools/poke.py seq N           replace it with "E6 N F1" (run NPC sequence N, end)
#   python tools/poke.py hex "e6 05 f1"  replace it with the given bytes
#   python tools/poke.py dsw OFF [VAL]   read (or write) a word at DS:OFF (DS taken from the dump default 22d0)
import sys, os, ctypes, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rpm
k32 = ctypes.windll.kernel32

def write(g, lin, data):
    n = ctypes.c_size_t()
    ok = k32.WriteProcessMemory(g.h, ctypes.c_void_p(g.membase + lin), data, len(data), ctypes.byref(n))
    return bool(ok) and n.value == len(data)

def main():
    g = rpm.Guest()
    c = sys.argv[1]
    needle = b'len rocks block the way.' + bytes([0])   # level 1: the script runs when the rubble at the start is clicked; the first 4 bytes (F8 'fal') get overwritten
    if c in ('find', 'seq', 'hex'):
        m = g.mem(0, 0xa0000)
        hits = []
        i = m.find(needle)
        while i >= 0:
            hits.append(i - 4); i = m.find(needle, i + 1)
        print('script message at', [hex(h) for h in hits])
        if c == 'find' or not hits: return
        data = bytes([0xe6, int(sys.argv[2], 0), 0xf1]) if c == 'seq' else bytes.fromhex(sys.argv[2])
        for h in hits:
            print(hex(h), 'write', data.hex(' '), write(g, h, data))
    elif c == 'dsw':
        ds = 0x22d0 * 16
        off = int(sys.argv[2], 16)
        if len(sys.argv) > 3:
            print(write(g, ds + off, struct.pack('<H', int(sys.argv[3], 0))))
        print(hex(g.w(ds + off)))

if __name__ == '__main__':
    main()
