# Gold Box の START.EXE（展開後）と GAME.OVR から、日本語化でフックする関数を命令列で探す
import sys, re, struct
img = open(sys.argv[1], 'rb').read()
ovr = open(sys.argv[2], 'rb').read()
def find(name, data, pat):
    hits = [m.start() for m in re.finditer(pat, data, re.S)]
    print(name, [hex(h) for h in hits])
    return hits
# 1 文字描画: push bp/mov bp,sp/sub sp,12/cmp [bp+10],27/ja/cmp [bp+e],18/ja/mov ax,[font]
d = find('draw', img, rb'\x55\x89\xe5\x83\xec\x12\x80\x7e\x10\x27\x77.\x80\x7e\x0e\x18\x77.\xa1(..)')
for h in d:
    print('  font ptr DS:%04x' % struct.unpack_from('<H', img, h + 19)[0])
# 1 行表示: ... lcall strcopy / cmp [bp+e],27 ... mov al,[bp-100]
find('print', img, rb'\x55\x89\xe5\x81\xec\x04\x01\xc4\x7e\x06\x06\x57\x8d\xbe\x00\xff\x16\x57\xb8\xff\x00\x50\x9a....\x80\x7e\x0e\x27')
# ワードラップ表示
w = find('wprint', img, rb'\x55\x89\xe5\x81\xec\x26\x01\xc4\x7e\x06\x06\x57\x8d\xbe\x00\xff\x16\x57\xb8\xff\x00\x50\x9a....\x80\x7e\x14\x27')
for h in w:
    k = img.find(b'\xc6\x86\xfd\xfe\x01\x8a\x86\x00\xff', h)
    print('  wprint mov al,[bp-100] at', hex(k + 5), 'cursor', img[h+0x36:h+0x3c].hex())
# 横並びメニュー: 項目を [bp-53] に写した直後の mov byte [xxxx],0
m = find('menu', ovr, rb'\x55\x89\xe5\x81\xec\x90\x00\xc4\x7e\x18\x06\x57\x8d\x7e\xd6\x16\x57\xb8\x28\x00\x50\x9a....\xc4\x7e\x14\x06\x57\x8d\x7e\xad\x16\x57\xb8\x28\x00\x50\x9a....\xc6\x06(..)\x00')
for h in m:
    print('  menu hook at', hex(h + 0x2d), 'var DS:%04x' % struct.unpack_from('<H', ovr, h + 0x2f)[0])
