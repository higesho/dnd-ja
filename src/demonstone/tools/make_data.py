# 訳 → build/data.bin（DLL の資源 1）と build/chars.txt（フォントに入れる字）
#   "DSTJ", u32 n, n x {u64 FNV-1a(英語 UTF-16LE), u32 英語の字数, u32 訳の字数, 訳 UTF-16LE}, u32 ncs, ncs x u16（訳に使う ASCII 以外の字）
import sys, os, json, glob, struct, re
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

def fnv64(b):
    h = 14695981039346656037
    for c in b: h = ((h ^ c) * 1099511628211) & 0xffffffffffffffff
    return h

def norm(t):
    out = []
    for ch in t:
        o = ord(ch)
        if 0xff01 <= o <= 0xff5e and ch not in '！？（）：、。・ー％＆':
            ch = chr(o - 0xfee0)
        if ch == '　': ch = ' '
        out.append(ch)
    return ''.join(out)

def load():
    tr = {}
    for f in sorted(glob.glob(os.path.join(ROOT, 'text', 'chunks', 'c*.ja.json'))):
        en = json.load(open(f.replace('.ja.json', '.en.json'), encoding='utf-8'))
        ja = json.load(open(f, encoding='utf-8'))
        for it in en['items']:
            t = ja.get(it['id'])
            if t and t != '=': tr[it['en']] = '' if t == '-' else norm(t)
    p = os.path.join(ROOT, 'text', 'fixes.json')
    if os.path.exists(p): tr.update({k: norm(v) for k, v in json.load(open(p, encoding='utf-8')).items()})
    return tr

if __name__ == '__main__':
    tr = load()
    out = b'DSTJ' + struct.pack('<I', len(tr))
    cs = set()
    for en, ja in tr.items():
        e = en.encode('utf-16le'); j = ja.encode('utf-16le')
        out += struct.pack('<QII', fnv64(e), len(en), len(ja)) + j
        cs.update(c for c in ja if ord(c) >= 0x80)
    cs = sorted(cs)
    out += struct.pack('<I', len(cs)) + b''.join(struct.pack('<H', ord(c)) for c in cs)
    os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
    open(os.path.join(ROOT, 'build', 'data.bin'), 'wb').write(out)
    open(os.path.join(ROOT, 'build', 'chars.txt'), 'w', encoding='utf-8').write(''.join(cs))
    print(len(tr), 'entries', len(cs), 'chars', len(out), 'bytes')
