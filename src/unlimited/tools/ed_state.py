# テスト用: エディタの入力欄の中身（記号と、日本語に戻した文）とカーソルの位置を表示する
#   python -X utf8 tools/ed_state.py 先頭の記号（例: "[3;"）
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
import rpm, uajp
g = rpm.Guest(); m = g.mem(0, 0xa0000)
key = sys.argv[1].lower().encode('latin1')
i = m.find(key)
if i < 0: i = m.find(key.upper())
if i < 0: raise SystemExit('buffer not found')
txt = m[i:i + 0xeb].split(b'\0')[0].decode('latin1')
p = m[i + 0xeb]
print('%s|%s   cursor %d of %d' % (uajp.decode(txt[:p]), uajp.decode(txt[p:]), p, len(txt)))
