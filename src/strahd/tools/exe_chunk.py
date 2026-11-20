# UI strings of CODE.1 / CODE.2 to translate in place -> text/exe/exe.en.txt  ("[id] (max N bytes) TEXT")
import json, os, re
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
GAME = os.path.join(ROOT, '..', 'GAME', 'RLOFT')
ex = json.load(open(os.path.join(ROOT, 'text', 'en', 'exe_strings.json'), encoding='utf8'))
DROP = re.compile(r'^(\$Wj| QVj|j<Pj|\d|GAME\.OPT|COLORS\.|Zero error|Divide by zero|Unknown error|Bad data|Too Many Objects|Quit $|'
                  r'Error unloading|No Space Available|Error trying|Unknown compression|Unable to open palette|Attempt to|Sprite file|'
                  r'Cannot recursively|Map %d|Menzoberranzan %s|Uhere|%\'\))')
items = [x for x in ex if x['file'] == 'CODE.2' and not DROP.search(x['en'])]
# CODE.1: the character-creation label table (MALE .. SELECT)
d = open(os.path.join(GAME, 'CODE.1'), 'rb').read()
a = d.index(b'\0MALE\0') + 1
b = d.index(b'SELECT\0', a) + 7
p = a
while p < b:
    e = d.index(b'\0', p)
    s = d[p:e].decode('latin1')
    if re.search(r'[A-Za-z]{2,}', s):
        room = ((e + 1 + 3) & ~3) - p - 1
        items.append({'id': 1000 + len(items), 'file': 'CODE.1', 'off': p, 'room': room, 'en': s})
    p = (e + 1 + 3) & ~3
    while p < b and d[p] == 0: p += 4
os.makedirs(os.path.join(ROOT, 'text', 'exe'), exist_ok=True)
json.dump(items, open(os.path.join(ROOT, 'text', 'exe', 'items.json'), 'w', encoding='utf8'), ensure_ascii=False, indent=0)
with open(os.path.join(ROOT, 'text', 'exe', 'exe.en.txt'), 'w', encoding='utf8') as f:
    for x in items:
        f.write('[%d] (max %d bytes) %s\n' % (x['id'], x['room'], x['en']))
print(len(items))
