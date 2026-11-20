# build/tr.json {english: japanese} + text/src.json -> build/tr.bin (TJP/1), build/files.txt (TJP/2), build/chars.txt
import os, json, struct
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
B = os.path.join(ROOT, 'build')


def fnv64(b):
    h = 0xcbf29ce484222325
    for c in b:
        h ^= c
        h = (h * 0x100000001b3) & 0xffffffffffffffff
    return h


def euc(s):
    s = s.replace('～', '〜').replace('－', '−').replace('—', '―').replace('∥', '‖')
    return s.encode('euc_jp', 'replace')


def main():
    tr = json.load(open(os.path.join(B, 'tr.json'), encoding='utf-8'))
    src = json.load(open(os.path.join(ROOT, 'text', 'src.json'), encoding='utf-8'))
    items, chars = {}, set()
    for en, ja in tr.items():
        k = fnv64(en.strip().encode('cp1252', 'replace'))
        items[k] = euc(ja)
        chars.update(c for c in ja if ord(c) >= 0x80)
    # per-file overrides: text/fixes_file.json {"mes/skill.mes": {"Heal": "治療"}} keyed on "path|english"
    fp = os.path.join(ROOT, 'text', 'fixes_file.json')
    perfile = json.load(open(fp, encoding='utf-8')) if os.path.exists(fp) else {}
    for path, d in perfile.items():
        for en, ja in d.items():
            items[fnv64((path.lower() + '|' + en.strip()).encode('cp1252', 'replace'))] = euc(ja)
            chars.update(c for c in ja if ord(c) >= 0x80)
    keys = sorted(items)
    hdr = 4 + 12 * len(keys)
    out = bytearray(struct.pack('<I', len(keys)))
    blob = bytearray()
    for k in keys:
        out += struct.pack('<III', k & 0xffffffff, k >> 32, hdr + len(blob))
        blob += items[k] + b'\0'
    open(os.path.join(B, 'tr.bin'), 'wb').write(bytes(out + blob))
    files = sorted({u['f'] for u in src if u['en'] in tr} | set(perfile))
    open(os.path.join(B, 'files.txt'), 'wb').write('\n'.join(files).encode())
    for a, b in ((0x3000, 0x30ff), (0xff01, 0xff5e)):
        chars.update(chr(c) for c in range(a, b + 1))
    open(os.path.join(B, 'chars.txt'), 'w', encoding='utf-8').write(''.join(sorted(chars)))
    print(len(keys), 'strings', len(files), 'files', len(chars), 'chars')


if __name__ == '__main__':
    main()
