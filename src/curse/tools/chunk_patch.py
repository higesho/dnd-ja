# 一度きりの書き換え: 続けて表示される断片（文の途中で切れた ECL）の注記と、空にする訳「-」
import sys
root = sys.argv[1]
p = root + '/tools/chunk.py'
s = open(p, encoding='utf-8').read()
old = """        items = []
        for i, (src, it, core) in enumerate(ch):
            d = {'id': '%02d-%03d' % (k, i), 'src': it['id'], 'en': core}
            nt = note_of(it['en'], it['kind'])"""
new = """        items = []
        prev_cont = False
        for i, (src, it, core) in enumerate(ch):
            d = {'id': '%02d-%03d' % (k, i), 'src': it['id'], 'en': core}
            nt = note_of(it['en'], it['kind'])
            if src == 'ecl':
                cont = it['en'].endswith(' ') and not re.search(r"[.!?'\\"]$", core)
                if prev_cont:
                    nt = '前の項目の続き（つないで 1 つの文）' + ('・' + nt if nt else '')
                if cont:
                    nt = (nt + '・' if nt else '') + '次の項目へ文が続く'
                prev_cont = cont"""
assert old in s
s = s.replace(old, new, 1)
open(p, 'w', encoding='utf-8').write(s)

p = root + '/tools/check_chunk.py'
s = open(p, encoding='utf-8').read()
s = s.replace("""    if j == '':
        continue""", """    if j == '' or j == '-':
        continue""", 1)
open(p, 'w', encoding='utf-8').write(s)

p = root + '/tools/make_patch.py'
s = open(p, encoding='utf-8').read()
old = """        en = info['en'].strip()
        if info['kind'] == 'menu':"""
new = """        en = info['en'].strip()
        if ja == '-':            # つないだ文の訳を前の項目に入れたので、この断片は何も出さない
            table[en.encode('latin1')] = ('', 0)
            continue
        if info['kind'] == 'menu':"""
assert old in s
s = s.replace(old, new, 1)
open(p, 'w', encoding='utf-8').write(s)
print('ok')
