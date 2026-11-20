# 訳す単位を text/chunks/*.json に分ける
#   文章: {"id", "paras": [英語の段落...], "note"} -> 訳は段落の数をそろえたリスト
#   EXE: {"id", "en", "max", "note"}（max = 最大字数）
import os, json, re
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
u = json.load(open(os.path.join(ROOT, 'text', 'units.json'), encoding='utf-8'))
os.makedirs(os.path.join(ROOT, 'text', 'chunks'), exist_ok=True)

DF_NOTE = {0: 'opening caption under the title (attract mode)'}
def df_note(i):
    return DF_NOTE.get(i, 'mission introduction story shown before a mission (narration to the player, "you")')
def gm_note(i):
    return 'mission result text (success or failure of a mission, or promotion), narration to the player'

KEYS = (0xa866, 0xabe9)
def exe_note(off, en):
    if 0x7853 <= off < 0x7dd3: return 'message bar during flight (20 cells, centered, short exclamation)'
    if 0x7c83 <= off < 0x7d55: return 'preferences menu item'
    if 0x8c3c <= off < 0x8c6c: return 'name of your dragon (proper name, katakana)'
    if 0x8c6c <= off < 0x8dd5: return 'enemy type (dragon age + color, or monster)'
    if 0x8e0e <= off < 0x8ff8: return 'rank in the Knights of Solamnia (Crown / Sword / Rose orders)'
    if 0x900a <= off < 0x9217: return 'mission name'
    if 0x95a5 <= off < 0x95f5: return 'title menu item'
    if 0xa70b <= off < 0xa84a: return 'redefine-keyboard screen: category or instruction line'
    if KEYS[0] <= off <= KEYS[1]: return 'keyboard list: translate ONLY the action name (the part before the key name); key name is kept'
    if 0x83f9 <= off < 0x8450: return 'mission result table label'
    return 'menu / prompt / label'

exe = []
for x in u['exe']:
    en = x['en']
    off = x['off']
    item = {'id': x['id'], 'en': en.strip(), 'note': exe_note(off, en)}
    if KEYS[0] <= off <= KEYS[1]:
        m = re.match(r'(.*?\S)\s{2,}(\S.*)$', en)
        item['en'] = m.group(1).strip(); item['key'] = m.group(2)
        item['max'] = 9
    else:
        item['max'] = max(2, x['len'] // 2 + (2 if x['len'] >= 12 else 0))
    exe.append(item)
json.dump(exe, open(os.path.join(ROOT, 'text', 'chunks', 'exe.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)

def text_chunks(units, prefix, note, n):
    per = (len(units) + n - 1) // n
    for k in range(n):
        part = [{'id': x['id'], 'paras': x['paras'], 'note': note(x['entry'])} for x in units[k * per:(k + 1) * per]]
        json.dump(part, open(os.path.join(ROOT, 'text', 'chunks', '%s%d.json' % (prefix, k + 1)), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
text_chunks(u['df'], 'df', df_note, 2)
text_chunks(u['game'], 'gm', gm_note, 3)
print('exe', len(exe), 'df', len(u['df']), 'game', len(u['game']))
