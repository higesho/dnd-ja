# START.EXE（展開後）と GAME.OVR から、コードが参照している Pascal 文字列定数を抜き出す
#   mov di,imm / push cs / push di  （bf xx xx 0e 57）→ cs:imm の文字列
import sys, os, re, struct, json
sys.path.insert(0, os.path.dirname(__file__))
import exepack
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
exe = open(os.path.join(ROOT, 'orig', json.load(open(os.path.join(ROOT, 'game.json'))).get('exe', 'START.EXE')), 'rb').read()
img, rel, regs = exepack.load(exe)
ovr = open(os.path.join(ROOT, 'orig', 'GAME.OVR'), 'rb').read()

def printable(b):
    return all(0x20 <= c < 0x7f for c in b)

def segs_img():
    """展開後イメージのコード区分の先頭（再配置される lcall の区分値から）"""
    relpos = set(seg * 16 + off for off, seg in rel)
    bases = set()
    for p in relpos:
        if p >= 3 and img[p - 3] == 0x9a:
            bases.add(struct.unpack_from('<H', img, p)[0] * 16)
    return sorted(b for b in bases if b < len(img))

def ovr_segs():
    out = []
    for m in re.finditer(rb'\xcd\x3f\x00\x00', img):
        a = m.start()
        if a % 16: continue
        off, csz, rsz, nent = struct.unpack_from('<IHHH', img, a + 4)
        if off < len(ovr) and csz:
            out.append((off, csz))
    return out

def scan(data, start, end, name, out):
    code = data[start:end]
    for m in re.finditer(rb'\xbf(..)\x0e\x57', code, re.S):
        imm = struct.unpack('<H', m.group(1))[0]
        a = start + imm
        if not (start <= a < end): continue
        n = data[a]
        s = data[a + 1:a + 1 + n]
        if n == 0 or a + 1 + n > end or not printable(s): continue
        key = '%s:%05x' % (name, a)
        out[key] = s.decode('latin1')

out = {}
bases = segs_img()
ds = regs and None
for i, b in enumerate(bases):
    e = bases[i + 1] if i + 1 < len(bases) else len(img)
    scan(img, b, e, 'EXE', out)
for off, csz in ovr_segs():
    scan(ovr, off, off + csz, 'OVR', out)
print(len(out), 'strings')
json.dump(out, open(os.path.join(ROOT, 'text', 'exe_strings.json'), 'w'), indent=0)
