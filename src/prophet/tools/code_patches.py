# Byte-pattern patches of CODE.1 (character creation) and CODE.2 (game) of Ravenloft: Stone Prophet.
# Both are optimised builds with the same text module. Returns [(file offset, old bytes, new bytes)].
# Callback numbers relative to CB_BASE: 0 width -> eax, 1 draw (push [esp+10h]), 2 draw+remap (push [esp+14h]).
import re
PARAMS = {}

def cb(base, k):
    return bytes([0xfe, 0x38, base + k, 0])

def find1(exe, pat, fn, what):
    hs = [m.start() for m in re.finditer(pat, exe, re.S)]
    assert len(hs) == 1, (fn, what, [hex(h) for h in hs])
    return hs[0]

def patches(fn, exe, base):
    P = []
    w = find1(exe, re.escape(bytes.fromhex('89f1803b00740d0fbe030fb6440104')), fn, 'strw') + 7
    for m in re.finditer(rb'\x0f\xb6([\x44\x54])([\x01\x02\x1a])\x04', exe[w - 0x80:w + 0x500]):
        f = w - 0x80 + m.start()
        P.append((f, exe[f:f + 5], cb(base, 0 if m.group(1) == b'\x44' else 3) + m.group(2)))
    assert len(P) == 11, (fn, len(P))
    d1 = find1(exe, re.escape(bytes.fromhex('837c24100074')) + b'.' + re.escape(bytes.fromhex('ff742410' * 4 + 'e8')), fn, 'draw1') + 7
    d2 = find1(exe, re.escape(bytes.fromhex('837c24100074')) + b'.' + re.escape(bytes.fromhex('ff742414' * 5 + 'e8')), fn, 'draw2') + 7
    P.append((d1, bytes.fromhex('ff742410'), cb(base, 1)))
    P.append((d2, bytes.fromhex('ff742414'), cb(base, 2)))
    # word wrap of the dialogue / text window: break at 0x1F instead of spaces
    scan = find1(exe, re.escape(bytes.fromhex('0fbe0383f820741c803b00')), fn, 'wrap scan') + 3
    sk = exe.rindex(bytes.fromhex('83f82075'), scan - 0x40, scan - 3)
    pu = scan + exe[scan:scan + 0x80].index(bytes.fromhex('6a20'))
    assert exe[pu + 2] == 0xe8 or exe[pu + 3] == 0xe8, (fn, hex(pu))
    P.append((sk, bytes.fromhex('83f820'), bytes.fromhex('83f81f')))
    P.append((scan, bytes.fromhex('83f820'), bytes.fromhex('83f81f')))
    P.append((pu, bytes.fromhex('6a20'), bytes.fromhex('6a1f')))
    if fn == 'CODE.2':
        # prose flag (k8x12): entry callback (cb 8) replaces push ebx/esi/edi/ebp and points the return address at a
        # trampoline callback (cb 9) in the zero padding after the preceding function
        st = exe.rindex(bytes.fromhex('5356575589e583ec14'), scan - 0x200, scan)
        pad = exe.rindex(bytes.fromhex('c3' + '00' * 10), st - 0x400, st) + 3
        P.append((st, bytes.fromhex('53565755'), cb(base, 8)))
        P.append((pad, bytes(4), cb(base, 9)))
        PARAMS['tramp_delta'] = pad - (st + 4)
    return P
