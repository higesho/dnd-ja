# build/patch.bin（winmm.dll に埋め込むデータ）を作る
#   python -X utf8 tools/make_patch.py      （HF_LOG=0 でログなし）
import sys, os, json, struct, glob
sys.path.insert(0, os.path.dirname(__file__))
from mp_helpers import *

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
BDF = os.path.join(ROOT, 'font', 'misaki_gothic_2nd.bdf')
CB_BASE = 0x70
LOG = int(os.environ.get('HF_LOG', '1'))

def load_translations():
    """text/ja.json（英語の断片 → 訳）と text/fixes.json（直し）"""
    tr = {}
    for f in sorted(glob.glob(os.path.join(ROOT, 'text', 'chunks', 'c*.ja.json'))):
        en = json.load(open(f.replace('.ja.json', '.en.json'), encoding='utf-8'))
        ja = json.load(open(f, encoding='utf-8'))
        for it in en['items']:
            t = ja.get(it['id'])
            if t and t != '=':
                tr[it['en']] = t
    p = os.path.join(ROOT, 'text', 'ja.json')
    if os.path.exists(p):
        tr.update(json.load(open(p, encoding='utf-8')))
    p = os.path.join(ROOT, 'text', 'fixes.json')
    if os.path.exists(p):
        tr.update(json.load(open(p, encoding='utf-8')))
    return tr

def main():
    codec = Codec()
    gl = load_bdf(BDF)
    sects = []
    table = load_translations()
    tb = struct.pack('<I', len(table))
    for en, ja in table.items():
        e = en.encode('latin1')
        jb = codec.encode(ja) if ja != '-' else b''
        tb += struct.pack('<BBHHQ', 0, 0, len(jb), len(e), fnv64(e)) + jb
    sects.append(sect('T', tb))
    codes, bits, flags = [], b'', b''
    for ch, cd in sorted(codec.map.items(), key=lambda kv: kv[1]):
        g = gl.get(ord(ch))
        if g is None:
            print('グリフなし', ch); g = bytes(8)
        codes.append(struct.unpack('>H', cd)[0]); bits += g
        flags += bytes([(1 if ch in NO_START else 0) | (2 if ch in NO_END else 0)])
    sects.append(sect('F', struct.pack('<I', len(codes)) + struct.pack('<%dH' % len(codes), *codes) + bits + flags))
    sects.append(sect('P', struct.pack('<2I', CB_BASE, LOG)))
    blob = b'HFJ1' + struct.pack('<II', 1, len(sects)) + b''.join(sects)
    os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
    open(os.path.join(ROOT, 'build', 'patch.bin'), 'wb').write(blob)
    print('patch.bin', len(blob), 'bytes,', len(table), 'entries,', len(codes), 'glyphs')

if __name__ == '__main__':
    main()
