# TEXT.DAT (u16 offs[250], strings with CR line breaks, NUL) -> text/en/text.json
# GAME.EXE UI strings (NUL terminated, in place) -> text/en/exe.json  {id, off, room, en}
import json, os, re, struct
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
GAME = os.path.join(ROOT, '..', 'GAME', 'SHADOW')

def load_text(path=None):
    d = open(path or os.path.join(GAME, 'TEXT.DAT'), 'rb').read()
    n = struct.unpack_from('<H', d, 0)[0] // 2
    offs = [struct.unpack_from('<H', d, 2 * i)[0] for i in range(n)] + [len(d)]
    return [d[offs[i]:offs[i + 1]].split(b'\0')[0].decode('latin1') for i in range(n)]

def build_text(strings):
    n = len(strings)
    body = b''
    offs = []
    for s in strings:
        offs.append(2 * n + len(body))
        body += s + b'\0'
    return b''.join(struct.pack('<H', o) for o in offs) + body

# UI strings worth translating (by content); file/debug names and data codes are left alone
KEEP_OUT = re.compile(r'\.(dat|drv|st|adv|adb|dir|___)$|^maped|^Test$|^game [a-j]|Lilley|^Graham|C_FILE_INFO|NMSG|run-time|Version UK|'
                      r'^[A-Z0-9 ]+$|^Sarah|[0-9][CFMRT](/[0-9][CFMRT])?$|No Character')

def exe_strings():
    d = open(os.path.join(GAME, 'GAME.EXE'), 'rb').read()
    out = []
    for m in re.finditer(rb'[\x20-\x7e\r]{3,}\x00', d):
        if m.start() < 0x10000 or m.start() > 0x19700: continue
        s = m.group()[:-1].decode('latin1')
        if not re.search(r'[A-Za-z]{3,}', s) or KEEP_OUT.search(s.strip()): continue
        if re.search(r'[^ -~\r]', s) or re.search(r'(.)\1\1\1', s): continue
        out.append({'id': len(out), 'off': m.start(), 'room': len(s), 'en': s})
    return out

if __name__ == '__main__':
    os.makedirs(os.path.join(ROOT, 'text', 'en'), exist_ok=True)
    t = load_text()
    json.dump([{'id': i, 'en': s} for i, s in enumerate(t)], open(os.path.join(ROOT, 'text', 'en', 'text.json'), 'w', encoding='utf8'), ensure_ascii=False, indent=0)
    e = exe_strings()
    json.dump(e, open(os.path.join(ROOT, 'text', 'en', 'exe.json'), 'w', encoding='utf8'), ensure_ascii=False, indent=0)
    print(len(t), len(e))
    for x in e: print(x['id'], x['room'], repr(x['en'])[:80])
