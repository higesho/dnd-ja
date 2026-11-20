# 翻訳する英文を text/en/*.json に書き出す（出どころ別、ゲーム内の順）
#   items: [{"id", "src", "en", "kind"}]   kind: text=普通の文 / menu=横並びメニューの項目（ホットキーの字は機械が付ける）
import sys, os, re, json, struct
sys.path.insert(0, os.path.dirname(__file__))
import exepack
from goldbox import dax
from extract_ecl import strings as ecl_strings
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.join(ROOT, 'test', 'game', [d for d in os.listdir(os.path.join(ROOT, 'test', 'game')) if os.path.isdir(os.path.join(ROOT, 'test', 'game', d))][0])
OUT = os.path.join(ROOT, 'text', 'en')
os.makedirs(OUT, exist_ok=True)
exe = open(os.path.join(ROOT, 'orig', json.load(open(os.path.join(ROOT, 'game.json'))).get('exe', 'START.EXE')), 'rb').read()
img, rel, regs = exepack.load(exe)
# データセグメント: 入口の最初の lcall 先（System の初期化）の mov dx,imm16 / mov ds,dx
def find_ds():
    rIP, rCS = regs[0], regs[1]
    a = rCS * 16 + rIP
    assert img[a] == 0x9a, hex(a)
    off, seg = struct.unpack_from('<HH', img, a + 1)
    b = seg * 16 + off
    assert img[b] == 0xba and img[b + 3:b + 5] == b'\x8e\xda', img[b:b + 5].hex()
    return struct.unpack_from('<H', img, b + 1)[0] * 16
DSOFF = find_ds()

seen = set()
def add(lst, id_, src, en, kind='text'):
    key = (kind, en)
    if key in seen or not re.search('[A-Za-z]', en):
        return
    seen.add(key)
    lst.append({'id': id_, 'src': src, 'en': en, 'kind': kind})

def save(name, lst):
    json.dump({'items': lst}, open(os.path.join(OUT, name + '.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    print(name, len(lst), sum(len(x['en']) for x in lst))

# 横並びメニュー（DS の定数と、コード中の定数）: 空白区切りの項目ごと
menus = []
MENU_RE = re.compile(r'^ *(?:[A-Za-z]*[A-Z][a-z]* *){2,}$')
def menu_words(s, src):
    for w in s.split():
        if re.fullmatch(r'[a-z]*[A-Z][a-z]*', w) and len(w) >= 2:
            add(menus, 'menu:' + w, src, w, 'menu')
# メニュー関数（3c5:002a）に渡される文字列（GAME.OVR の呼び出し箇所から）
CALLSITE_MENUS = ['Heal View Pool Appraise Exit', 'Money Items Exit', 'View Take Pool', 'Buy View Take Pool Share Appraise Exit',
    'Buy View Pool Appraise Exit', 'Quick Done', 'Area Cast View Encamp Search Look', 'Cast View Encamp Search Look', ' Exit',
    'Cast Memorize Scribe Display Rest Exit', 'Order Drop Speed Icon Pics Exit', 'Save View Magic Rest Alter Exit', 'Keep Exit',
    'Head Body Keep', 'Rest   daYs Hours Mins   Inc Dec   Exit', 'Sell Keep', 'Parts color-1 color-2 Size Exit', 'Head Weapon Exit',
    'Weapon Body xxxx Shield Arm Leg Exit', ' Keep Exit', 'Next Prev Keep Exit', 'Select Exit', 'Place Exit',
    'Move View Aim Use Cast Turn Quick Done', 'Combat Wait Flee Parlay']

for m in (CALLSITE_MENUS if 'POOLRAD' in GAME else []):
    menu_words(m, 'MENU')

# 1) コード中の文字列定数
code = json.load(open(os.path.join(ROOT, 'text', 'exe_strings.json')))
lst = []
for k, s in code.items():
    if MENU_RE.match(s) and (s.endswith(' ') or len(s.split()) >= 2) and all(len(w) <= 9 for w in s.split()):
        menu_words(s, k)
    add(lst, k, 'EXE' if k.startswith('EXE') else 'OVR', s)
save('code', lst)

# 2) データセグメント（呪文・職業・種族・アイテム名の部品・メニューなど）
ds = img[DSOFF:]
lst = []
for m in re.finditer(rb'[\x20-\x7e]{2,}', ds):
    a = m.start(); n = ds[a - 1]
    if n < 2 or n > len(m.group()): continue
    s = ds[a:a + n].decode('latin1')
    if not re.search('[A-Za-z]{2}', s): continue
    if re.fullmatch(r'[a-z]+|[A-Z]{1,3}[a-z]?|N+|.*(?:ccc|tempgone|xxxx).*', s): continue
    if MENU_RE.match(s) and len(s.split()) >= 2 and all(len(w) <= 9 for w in s.split()):
        menu_words(s, 'DS:%04x' % a)
    add(lst, 'DS:%04x' % a, 'DS', s)
save('data', lst)

# 3) 怪物・NPC の名前
lst = []
for f in sorted(os.listdir(GAME)):
    if re.fullmatch(r'MON\dCHA\.DAX', f):
        for k, v in sorted(dax(os.path.join(GAME, f)).items()):
            s = v[1:1 + v[0]].decode('latin1')
            add(lst, '%s:%d' % (f[:7], k), 'NAME', s)
save('names', lst)

# 4) 台本（ECL）
lst = []
for f in sorted(os.listdir(GAME)):
    if not re.fullmatch(r'ECL\d\.DAX', f): continue
    for k, v in sorted(dax(os.path.join(GAME, f)).items()):
        for p, s in ecl_strings(v):
            add(lst, '%s:%d:%05x' % (f[:4], k, p), f[:4], s)
save('ecl', lst)

# ECL の選択肢（1 語の大文字）は、ゲームが「Combat Wait」のように頭だけ大文字にして横並びメニューにする
for it in lst:
    s = it['en']
    if re.fullmatch(r'[A-Z]+', s) and len(s) <= 10:
        add(menus, 'menu:' + s.capitalize(), it['id'], s.capitalize(), 'menu')
save('menu', menus)
