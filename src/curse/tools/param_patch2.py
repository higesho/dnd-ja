# 一度きりの書き換え: make_patch.py が作品ごとの値を _jp/game.json から読むようにする
p = 'tools/make_patch.py'
s = open(p, encoding='utf-8').read()
def rep(old, new):
    global s
    assert old in s, old[:60]
    s = s.replace(old, new, 1)

rep("""LOG = int(os.environ.get('PRJ_LOG', '1'))
""", """LOG = int(os.environ.get('PRJ_LOG', '1'))
GAMECFG = json.load(open(os.path.join(ROOT, 'game.json'), encoding='utf-8'))
H = lambda k: int(GAMECFG[k], 16)
""")
rep("""    SEG = 0x7090                       # 709:0000
    patch(SEG + 0x064f, '8a8600ff', cb(CB_PRINT))           # 1 行表示: mov al,[bp-100]
    patch(SEG + 0x07f5, '8a8600ff', cb(CB_WPRINT))          # ワードラップ表示: mov al,[bp-100]
    patch(SEG + 0x01e0, '89e583ec12', cb(CB_DRAW) + b'\\x90')  # 1 文字描画: mov bp,sp / sub sp,12""",
"""    patch(H('print_hook'), '8a8600ff', cb(CB_PRINT))            # 1 行表示: mov al,[bp-100]
    patch(H('wprint_hook'), '8a8600ff', cb(CB_WPRINT))          # ワードラップ表示: mov al,[bp-100]
    patch(H('draw_func') + 1, '89e583ec12', cb(CB_DRAW) + b'\\x90')  # 1 文字描画: mov bp,sp / sub sp,12""")
rep("""    opatch(0x2ccb9, 'c6062c6b00', cb(CB_MENU))               # 横並びメニュー: mov byte [6b2c],0""",
"""    mv = H('menu_var')
    opatch(H('menu_hook'), 'c606%02x%02x00' % (mv & 0xff, mv >> 8), cb(CB_MENU))   # 横並びメニュー: mov byte [var],0""")
rep("""    sects.append(sect('P', struct.pack('<II', CB_BASE, LOG)))""",
"""    sects.append(sect('P', struct.pack('<9I', CB_BASE, LOG, H('font_var'), H('cursor_var'), H('wprint_x1'), H('wprint_x2'),
                                       H('print_x'), H('menu_var'), GAMECFG.get('copy_answer', 0))
                      + GAMECFG['dirname'].encode().ljust(32, b'\\0')))""")
s = s.replace("""SPLIT_FIRST = ['Create New Character',""", """SPLIT_FIRST = GAMECFG.get('split_first') or ['Create New Character',""")
open(p, 'w', encoding='utf-8').write(s)
print('ok')
