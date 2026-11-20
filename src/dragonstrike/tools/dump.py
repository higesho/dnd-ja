# DOSBox のメモリ 1MB を書き出させる（DLL のテスト機能）-> work/mem.bin
import mmap, struct, time, os, shutil
m = mmap.mmap(-1, 16, tagname='DS_DUMP')
struct.pack_into('<i', m, 0, 1)
t = time.time()
while struct.unpack_from('<i', m, 0)[0] != 2:
    if time.time() - t > 20: raise SystemExit('dump timeout')
    time.sleep(0.1)
cs, ds = struct.unpack_from('<ii', m, 4)
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
shutil.copy(os.path.join(os.environ['LOCALAPPDATA'], 'DS_JP', 'mem.bin'), os.path.join(ROOT, 'work', 'mem.bin'))
print('cs %05x ds %05x' % (cs, ds))
