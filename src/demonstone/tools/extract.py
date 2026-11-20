# 翻訳する英文を抜き出す → text/units.json [[出どころ, 英文], ...]（重複なし、ゲーム内の順）
#   UI\*.OBX（画面の文字。Credits は人名が中心なので英語のまま）、subtitle\*.OBX（会話・ムービーの字幕）
import sys, os, json, glob, re
sys.path.insert(0, os.path.dirname(__file__))
import obx
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.join(ROOT, '..')
UI = ['Shell', 'Level', 'Boot', 'ItemName', 'ItemDesc', 'ItemDesig', 'SkillName', 'SkillDesc']

def want(s):
    t = re.sub(r'###[A-Z_]+', '', s)
    return bool(re.search(r'[A-Za-z]{2}', t))

if __name__ == '__main__':
    seen = set(); units = []
    files = [os.path.join(GAME, 'UI', n + '.OBX') for n in UI] + sorted(glob.glob(os.path.join(GAME, 'subtitle', '*.OBX')))
    for f in files:
        kind, ss = obx.read(f)
        tag = ('UI:' if kind == 'OBSS' else 'SUB:') + os.path.splitext(os.path.basename(f))[0]
        for i, s in ss:
            if not want(s) or s in seen: continue
            seen.add(s); units.append(['%s:%d' % (tag, i), s])
    json.dump(units, open(os.path.join(ROOT, 'text', 'units.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    print(len(units), 'units', sum(len(e) for _, e in units), 'chars')
