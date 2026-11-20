# Dump each thread's EIP and code-looking stack words of the running 32-bit test game (WOW64)
import sys, os, ctypes, struct, ctypes.wintypes as W
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
pid = int(open(os.path.join(ROOT, 'work', 'pid.txt')).read())
k = ctypes.windll.kernel32
k.OpenThread.restype = ctypes.c_void_p
k.OpenProcess.restype = ctypes.c_void_p
k.CreateToolhelp32Snapshot.restype = ctypes.c_void_p
class TE(ctypes.Structure):
    _fields_ = [('dwSize', W.DWORD), ('cntUsage', W.DWORD), ('th32ThreadID', W.DWORD), ('th32OwnerProcessID', W.DWORD),
                ('tpBasePri', W.LONG), ('tpDeltaPri', W.LONG), ('dwFlags', W.DWORD)]
hp = ctypes.c_void_p(k.OpenProcess(0x0410, False, pid))
snap = ctypes.c_void_p(k.CreateToolhelp32Snapshot(4, 0))
te = TE(); te.dwSize = ctypes.sizeof(TE)
tids = []
ok = k.Thread32First(snap, ctypes.byref(te))
while ok:
    if te.th32OwnerProcessID == pid: tids.append(te.th32ThreadID)
    ok = k.Thread32Next(snap, ctypes.byref(te))
lo, hi = 0x401000, 0x530000
nwords = int(sys.argv[1]) if len(sys.argv) > 1 else 400
for tid in tids:
    ht = ctypes.c_void_p(k.OpenThread(0x0002 | 0x0008 | 0x0040, False, tid))
    k.SuspendThread(ht)
    ctx = ctypes.create_string_buffer(0x2cc)
    struct.pack_into('<I', ctx, 0, 0x10001 | 0x10002)
    if not k.Wow64GetThreadContext(ht, ctx): print(tid, 'ctx fail'); k.ResumeThread(ht); continue
    ebp, eip, esp = struct.unpack_from('<I', ctx, 0xb4)[0], struct.unpack_from('<I', ctx, 0xb8)[0], struct.unpack_from('<I', ctx, 0xc4)[0]
    buf = ctypes.create_string_buffer(nwords * 4); got = ctypes.c_size_t()
    k.ReadProcessMemory(hp, ctypes.c_void_p(esp), buf, nwords * 4, ctypes.byref(got))
    k.ResumeThread(ht)
    ws = struct.unpack('<%dI' % (got.value // 4), buf.raw[:got.value])
    code = ['%x' % w for w in ws if lo <= w < hi]
    print('tid %d eip %08x esp %08x: %s' % (tid, eip, esp, ' '.join(code[:30])))
