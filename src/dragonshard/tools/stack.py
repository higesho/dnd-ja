# Sample where the (32-bit) game's threads are: EIP and return addresses into Dragonshard.exe on the stack.
#   python tools/stack.py [pid]
import ctypes, ctypes.wintypes as W, os, struct, sys
k = ctypes.windll.kernel32
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
pid = int(sys.argv[1]) if len(sys.argv) > 1 else int(open(os.path.join(ROOT, 'work', 'pid.txt')).read())

class TE(ctypes.Structure):
    _fields_ = [('dwSize', W.DWORD), ('cntUsage', W.DWORD), ('th32ThreadID', W.DWORD), ('th32OwnerProcessID', W.DWORD),
                ('tpBasePri', W.LONG), ('tpDeltaPri', W.LONG), ('dwFlags', W.DWORD)]

class CTX(ctypes.Structure):  # WOW64_CONTEXT
    _fields_ = [('ContextFlags', W.DWORD), ('Dr', W.DWORD * 6), ('Float', ctypes.c_byte * 112),
                ('SegGs', W.DWORD), ('SegFs', W.DWORD), ('SegEs', W.DWORD), ('SegDs', W.DWORD),
                ('Edi', W.DWORD), ('Esi', W.DWORD), ('Ebx', W.DWORD), ('Edx', W.DWORD), ('Ecx', W.DWORD), ('Eax', W.DWORD),
                ('Ebp', W.DWORD), ('Eip', W.DWORD), ('SegCs', W.DWORD), ('EFlags', W.DWORD), ('Esp', W.DWORD), ('SegSs', W.DWORD),
                ('Ext', ctypes.c_byte * 512)]

snap = k.CreateToolhelp32Snapshot(4, 0)
te = TE(); te.dwSize = ctypes.sizeof(TE)
tids = []
ok = k.Thread32First(snap, ctypes.byref(te))
while ok:
    if te.th32OwnerProcessID == pid: tids.append(te.th32ThreadID)
    ok = k.Thread32Next(snap, ctypes.byref(te))
hp = k.OpenProcess(0x1F0FFF, False, pid)
for tid in tids:
    ht = k.OpenThread(0x1F03FF, False, tid)
    k.Wow64SuspendThread(ht)
    c = CTX(); c.ContextFlags = 0x10007
    k.Wow64GetThreadContext(ht, ctypes.byref(c))
    buf = ctypes.create_string_buffer(0x2000); n = ctypes.c_size_t()
    k.ReadProcessMemory(hp, ctypes.c_void_p(c.Esp), buf, 0x2000, ctypes.byref(n))
    k.ResumeThread(ht); k.CloseHandle(ht)
    words = struct.unpack_from('<%dI' % (n.value // 4), buf.raw)
    # return addresses that follow a CALL instruction in the exe
    exe = open(os.path.join(ROOT, '..', 'Dragonshard.exe'), 'rb').read()
    def is_ret(v):
        if not (0x401000 + 6 <= v < 0x8de000): return False
        o = v - 0x400000  # .text raw offset == rva for this exe
        return exe[o - 5] == 0xe8 or exe[o - 6] == 0xff or exe[o - 2] == 0xff or exe[o - 3] == 0xff
    rets = [v for v in words if is_ret(v)]
    print('tid %5d eip %08x  rets %s' % (tid, c.Eip, ' '.join('%x' % v for v in rets[:16])))
