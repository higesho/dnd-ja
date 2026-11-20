# 一度きりの書き換え: extract_all.py / extract_ecl.py を作品に依存しない形にする（POOLRAD/_jp にも当てる）
import sys
root = sys.argv[1]
p = root + '/tools/extract_all.py'
s = open(p, encoding='utf-8').read()
s = s.replace("GAME = os.path.join(ROOT, 'test', 'game', 'POOLRAD')",
              "GAME = os.path.join(ROOT, 'test', 'game', [d for d in os.listdir(os.path.join(ROOT, 'test', 'game')) if os.path.isdir(os.path.join(ROOT, 'test', 'game', d))][0])")
s = s.replace("DSOFF = 0xc7c0", """# データセグメント: 入口の最初の lcall 先（System の初期化）の mov dx,imm16 / mov ds,dx
def find_ds():
    rIP, rCS = regs[0], regs[1]
    a = rCS * 16 + rIP
    assert img[a] == 0x9a, hex(a)
    off, seg = struct.unpack_from('<HH', img, a + 1)
    b = seg * 16 + off
    assert img[b] == 0xba and img[b + 3:b + 5] == b'\\x8e\\xda', img[b:b + 5].hex()
    return struct.unpack_from('<H', img, b + 1)[0] * 16
DSOFF = find_ds()""")
s = s.replace("for m in CALLSITE_MENUS:", "for m in (CALLSITE_MENUS if 'POOLRAD' in GAME else []):")
open(p, 'w', encoding='utf-8').write(s)
p = root + '/tools/extract_ecl.py'
s = open(p, encoding='utf-8').read()
s = s.replace("GAME = os.path.join(ROOT, 'test', 'game', 'POOLRAD')",
              "GAME = os.path.join(ROOT, 'test', 'game', [d for d in os.listdir(os.path.join(ROOT, 'test', 'game')) if os.path.isdir(os.path.join(ROOT, 'test', 'game', d))][0])")
open(p, 'w', encoding='utf-8').write(s)
print('ok')
