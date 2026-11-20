# EXE とデータファイルから訳す文字列を抜き出す -> text/units.json
#   exe: {"id": "x%05x"(VA), "va", "en"}   data: {"id": "d%03d", "en", "files"}
import sys, os, re, json, glob
sys.path.insert(0, os.path.dirname(__file__))
from le import LE
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.join(ROOT, '..', 'GAME', 'FANTASY')
e = LE()

JUNK_RE = re.compile(rb'(\.(GIF|gif|DAT|dat|CAS|MAP|XMI|xmi|cfg|msg|dmp|SK)$)|(^[a-z])|(%d\.)|Cannot|Corrupt|memory|Error|error|file|File|buffer|BLASTER|stdpatch|Mel |Trap|DOS/|DOS16|^NO SUCH FILE')
# 名前（スタッフ・キー名など）は訳さない
KEEP = set()
def is_text(s):
    if len(re.findall(rb'[A-Za-z]', s)) < 2: return False
    if JUNK_RE.search(s): return False
    if sum(c in b'abcdefghijklmnopqrstuvwxyz' for c in s) > len(s) * 0.3: return False
    return True

exe = []
seen = set()
def add_exe(va, s):
    if va in seen: return
    seen.add(va)
    exe.append({'id': 'x%05x' % va, 'va': va, 'en': s.decode('latin-1')})

intro = e.read(0x10190, 0x183).split(b'\0')[0]
add_exe(0x10190, intro)
img = bytes(e.img[1][:e.objs[1]['vsize']]); base = e.objs[1]['base']
for m in re.finditer(rb'[\x20-\x7e\n]{2,}(?=\x00)', img):
    va = base + m.start(); s = m.group()
    if not (0x60000 <= va < 0x67300): continue
    # 前が文字列の途中なら（printable が続いている）、その先頭から始まる
    if is_text(s): add_exe(va, s)

data = {}
files = ['GAZE.DAT', 'NPC1.DAT', 'SCENARIO.DAT', 'GAZGAMES.DAT'] + sorted(glob.glob(os.path.join(GAME, 'SCEN?.DAT'))) + sorted(glob.glob(os.path.join(GAME, 'SC?.DAT')))
for fn in files:
    d = open(os.path.join(GAME, os.path.basename(fn)), 'rb').read()
    if re.fullmatch(r'SC\d\.DAT', os.path.basename(fn)): d = d.split(b'\0')[0]   # シナリオ説明: 最初の終端まで（後ろは前の文の残り）
    for m in re.finditer(rb'[\x20-\x7e\n]{3,}', d):
        s = m.group()
        if re.search(rb'[A-Z]{2}', s) and not re.search(rb'ddd', s) and not re.fullmatch(rb'[\n !-/0-9]+', s):
            s = s.strip(b'\n')
            data.setdefault(s, []).append(os.path.basename(fn))
units = {'exe': exe, 'data': [{'id': 'd%03d' % i, 'en': s.decode('latin-1'), 'files': sorted(set(f))} for i, (s, f) in enumerate(data.items())]}
json.dump(units, open(os.path.join(ROOT, 'text', 'units.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
print('exe', len(exe), sum(len(x['en']) for x in exe), 'data', len(units['data']), sum(len(x['en']) for x in units['data']))
