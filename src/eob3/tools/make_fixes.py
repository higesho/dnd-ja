# Hand fixes on top of the chunk translations -> text/fixes.json
import json, os, re, sys
sys.path.insert(0, os.path.dirname(__file__))
from merge import unesc
ROOT = os.path.join(os.path.dirname(__file__), '..')

def get(i):
    for k in range(20):
        f = os.path.join(ROOT, 'text', 'chunks', 'c%02d.ja.txt' % k)
        if not os.path.exists(f):
            continue
        for l in open(f, encoding='utf-8'):
            m = re.match(r'^\[(\d+)\] ?(.*)$', l.rstrip('\n'))
            if m and int(m.group(1)) == i:
                return unesc(m.group(2))

fx = {}
fx[1062] = get(1062).lstrip('\n')          # English has no leading newline
fx[1063] = get(1063).lstrip('\n')
fx[620] = 'OK  '
fx[621] = '次へ  '
fx[648] = '届か'                            # CAN'T / REACH (two lines over the hand)
fx[649] = 'ない'
fx[653] = '弾'                              # NO / AMMO
fx[654] = '切れ'
fx[638] = '\n中に入る……'
fx[639] = '\n外に出る……'
json.dump({str(k): v for k, v in sorted(fx.items())}, open(os.path.join(ROOT, 'text', 'fixes.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
print(len(fx), 'fixes')
