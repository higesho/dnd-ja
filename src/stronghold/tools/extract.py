# STRONG.EXE の DGROUP（ファイル 0x15f00、段 0x1410）から訳す文字列を抜き出す -> text/units.json
import sys, os, re, json
sys.path.insert(0, os.path.dirname(__file__))
from mz import MZ
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
m = MZ()
DG = 0x15f00
seg = m.d[DG:0x1f420]
units = []
for x in re.finditer(rb'(?<![\x20-\x7e\n])[\x20-\x7e\n]{2,}(?=\x00)', seg):
    s = x.group()
    o = x.start()
    if re.search(rb'\.(lbm|xmi|voc|com|adv|ad|txt|dat|cfg|fli|sav|\d\d\d)$', s, re.I): continue
    if re.search(rb'%[0-9]*l?x|%d %d|%u %u|getcache|SIZESTASH|MAXSQUARE|error\(|Error \d|x:%d|^[A-Z]{4,}[A-Z0-9]*$', s): continue
    if len(re.findall(rb'[a-zA-Z]', s)) < 2: continue
    if o < 0x169 or 0x5c7 <= o < 0x6b0 or 0x5e80 <= o < 0x6200: continue   # 先頭はランタイムの文字列、0x5c7- はコピープロテクトの答えの語（英語で入力する）
    units.append({'id': 'x%04x' % o, 'off': o, 'en': s.decode('latin-1')})
json.dump(units, open(os.path.join(ROOT, 'text', 'units.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
print(len(units), sum(len(u['en']) for u in units))
