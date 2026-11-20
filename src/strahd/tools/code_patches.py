# Byte-pattern patches of CODE.1 (character creation, frame-pointer build) and CODE.2 (game) of Ravenloft: Strahd's Possession.
# Returns [(file offset, old bytes, new bytes)]. Callback numbers are relative to CB_BASE:
#   0 width -> eax, 1 draw (esp args, push [esp+10h]), 2 draw+remap (push [esp+14h]),
#   3 width -> edx, 4 draw (ebp frame, mov eax,[ebp+14h]; push eax), 5 draw+remap (mov eax,[ebp+18h]; push eax)
import re

def cb(base, k):
    return bytes([0xfe, 0x38, base + k, 0])

def find1(exe, pat, fn, what):
    hs = [m.start() for m in re.finditer(pat, exe, re.S)]
    assert len(hs) == 1, (fn, what, [hex(h) for h in hs])
    return hs[0]

def width_sites(exe, lo, hi, base):
    out = []
    for m in re.finditer(rb'\x0f\xb6([\x44\x54])([\x01\x02\x1a])\x04', exe[lo:hi]):
        f = lo + m.start()
        k = 0 if m.group(1) == b'\x44' else 3
        out.append((f, exe[f:f + 5], cb(base, k) + m.group(2)))
    return out

def patches(fn, exe, base):
    P = []
    if fn == 'CODE.2':
        w = find1(exe, re.escape(bytes.fromhex('89f1803b00740d0fbe030fb6440104')), fn, 'strw') + 7
        P += width_sites(exe, w - 0x80, w + 0x480, base)
        assert len(P) == 11, (fn, len(P))
        d1 = find1(exe, re.escape(bytes.fromhex('837c24100074')) + b'.' + re.escape(bytes.fromhex('ff742410' * 4 + 'e8')), fn, 'draw1') + 7
        d2 = find1(exe, re.escape(bytes.fromhex('837c24100074')) + b'.' + re.escape(bytes.fromhex('ff742414' * 5 + 'e8')), fn, 'draw2') + 7
        P.append((d1, bytes.fromhex('ff742410'), cb(base, 1)))
        P.append((d2, bytes.fromhex('ff742414'), cb(base, 2)))
        # word wrap of the dialogue window: break at 0x1F instead of spaces
        a = find1(exe, re.escape(bytes.fromhex('0fbe450083f8207503')), fn, 'wrap skip') + 4
        b = find1(exe, re.escape(bytes.fromhex('0fbe0383f8207419803b00')), fn, 'wrap scan') + 3
        c = b + exe[b:b + 0x60].index(bytes.fromhex('576a20e8')) + 1
        P.append((a, bytes.fromhex('83f820'), bytes.fromhex('83f81f')))
        P.append((b, bytes.fromhex('83f820'), bytes.fromhex('83f81f')))
        P.append((c, bytes.fromhex('6a20'), bytes.fromhex('6a1f')))
        # prose flag: callbacks at the entry (push ebx/esi/edi/ebp) and exit (pop ebp/edi/esi/ebx) of the word-wrap function;
        # text drawn while it runs uses the k8x12 glyphs
        st = exe.rindex(bytes.fromhex('5356575583ec14'), b - 0x200, b)
        ex = exe.index(bytes.fromhex('8b44240883c4145d5f5e5bc3'), b)     # 'mov eax,[esp+8]' of the function's own exit (the pops are shared)
        P.append((st, bytes.fromhex('53565755'), cb(base, 6)))
        P.append((ex, bytes.fromhex('8b442408'), cb(base, 7)))
        # copy protection: pass the answer (record +0Ch) instead of the ordinal (+08h) to the question format
        q = find1(exe, re.escape(bytes.fromhex('ffb7d4820000ffb7d0820000ffb7cc82000068bc0c0000')), fn, 'copy protection')
        P.append((q, bytes.fromhex('ffb7d4820000'), bytes.fromhex('ffb7d8820000')))
    elif fn == 'CODE.1':
        w = find1(exe, re.escape(bytes.fromhex('8b55080fbe120fb6541a0401d08b5508ff4508ebdd')), fn, 'strw') + 3
        P += width_sites(exe, w - 0x10, w + 0x450, base)
        assert len(P) == 8, (fn, len(P))
        d1 = find1(exe, re.escape(bytes.fromhex('837d140074')) + b'.' + re.escape(bytes.fromhex('8b4514508b4510508b450c508b450850e8')), fn, 'draw1') + 6
        d2 = find1(exe, re.escape(bytes.fromhex('837d140074')) + b'.' + re.escape(bytes.fromhex('8b4518508b4514508b4510508b450c50')), fn, 'draw2') + 6
        P.append((d1, bytes.fromhex('8b451450'), cb(base, 4)))
        P.append((d2, bytes.fromhex('8b451850'), cb(base, 5)))
    return P
