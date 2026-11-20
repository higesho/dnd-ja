# テスト中の DOSBox のレジスタと BP の鎖（呼び出し元）を表示する
import sys, os, struct
sys.path.insert(0, os.path.dirname(__file__))
import rpm
g = rpm.Guest()
r, s = g.regs()
names = ['ax', 'cx', 'dx', 'bx', 'sp', 'bp', 'si', 'di', 'ip']
print(' '.join('%s=%04x' % (n, v & 0xffff) for n, v in zip(names, r)))
sn = ['es', 'cs', 'ss', 'ds', 'fs', 'gs']
print(' '.join('%s=%04x' % (n, v) for n, v in zip(sn, s)))
ss = s[2] << 4
bp = r[5] & 0xffff
print('stack top:', g.mem(ss + (r[4] & 0xffff), 32).hex())
for i in range(14):
    if bp == 0: break
    nbp, ip, cs = struct.unpack('<3H', g.mem(ss + bp, 6))
    print('bp=%04x ret %04x:%04x lin %05x  args %s' % (bp, cs, ip, (cs << 4) + ip, g.mem(ss + bp + 6, 16).hex()))
    if nbp <= bp: break
    bp = nbp
