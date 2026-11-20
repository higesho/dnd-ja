# 翻訳する英文を抜き出す → text/units.json [[出どころ, 英文], ...]（ゲーム内の順、重複なし）
#   GPL スクリプトの文（GPLS/GPLnn.COD、7 ビット詰め）
#   GENIER.EXE の画面の文字列（下の EXE_RANGES の範囲）
import sys, os, re, json, glob
sys.path.insert(0, os.path.dirname(__file__))
import gpltext
from le import LE
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.join(ROOT, '..', 'GAME', 'GENIE')

# 画面に出る EXE の文字列の範囲（リンク時の番地）
EXE_RANGES = [
    (0x60310, 0x603c9),   # レベルアップ・旅の終わり
    (0x604c2, 0x6060e),   # 合言葉の確認（コピープロテクト）・巻物
    (0x6060f, 0x6061a),   # Exit / Empty
    (0x606ee, 0x60702),   # Loading... / Saving...
    (0x60703, 0x6073a),   # 能力値
    (0x60910, 0x60c2e),   # アイテム・呪文
    (0x60c2f, 0x60c44),   # The Genie's Curse / by
    (0x60c5c, 0x60f98),   # クレジットの見出し（! で始まるもの）
    (0x60ff1, 0x61066),   # 難しさ・名前・能力値
    (0x610d6, 0x610e5),   # Saved game N
    (0x6440a, 0x64413),   # Hi, I'm
]


def gpl_strings():
    out = []
    files = sorted(glob.glob(os.path.join(GAME, 'GPLS', 'GPL*.COD')), key=lambda f: int(re.findall(r'\d+', os.path.basename(f))[0]))
    for f in files:
        n = int(re.findall(r'\d+', os.path.basename(f))[0])
        for p, q, s in gpltext.scan(open(f, 'rb').read()):
            out.append(('GPL%03d:%05x' % (n, p), s.decode('latin1')))
    return out


def exe_strings():
    L = LE()
    img = bytes(L.img[1])
    out = []
    for a, b in EXE_RANGES:
        for m in re.finditer(rb'[\x20-\x7e]+', img[a - 0x60000:b - 0x60000]):
            s = m.group().decode('latin1')
            if not re.search(r'[A-Za-z]{2}', s): continue
            if a == 0x60c5c and not s.startswith('!'): continue      # クレジットの人名・会社名は英語のまま
            out.append(('EXE:%05x' % (a + m.start()), s))
    return out


if __name__ == '__main__':
    seen = set(); units = []
    for src, en in gpl_strings() + exe_strings():
        if en in seen: continue
        seen.add(en); units.append([src, en])
    json.dump(units, open(os.path.join(ROOT, 'text', 'units.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    print(len(units), 'units', sum(len(e) for _, e in units), 'chars')
