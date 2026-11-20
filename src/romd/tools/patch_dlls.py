"""sneg.dll / ddraw.dll を日本語表示用にパッチした複製を作る。

元の DLL は書き換えず、出力先フォルダにパッチ済みファイルを書き出す。
（v1.4.0.29 の SNEG 版向け。元バイト列が一致しない場合は中止する）

sneg.dll  : CJK 描画経路のコードページ 950(Big5) を 932(Shift-JIS) に変更し、
            フォールバック用 GDI フォントを日本語フォントにする。
ddraw.dll : UI(ImGui) が読み込む CJK 文字一覧のパスとフォントファイルを日本語用にする。

使い方: python patch_dlls.py <ゲームフォルダ> <出力フォルダ>
"""
import os
import struct
import sys

# ---- 設定（必要ならここを変える）
GDI_FACE = "Meiryo"            # ゲーム本文の CJK 文字を描く GDI フォント（19 文字以内）
GDI_FALLBACK_FACE = "MS Gothic"  # 上が無い場合（14 文字以内）
UI_FONT = "meiryo.ttc"         # UI 用フォント（%WINDIR%\Fonts 内, 11 文字以内）
UI_FONT_FALLBACK = "YuGothM.ttc"  # 同上フォールバック（11 文字以内）
CJK_LIST = "languages/ja/sneg_ui_cjk_characters.txt"  # 44 文字以内


def text_off(va):  # .text の VA -> ファイルオフセット（両 DLL とも同じ配置）
    return va - 0x10001000 + 0x400


def u16z(s, size):
    b = s.encode("utf-16le") + b"\0\0"
    assert len(b) <= size, s
    return b + b"\0" * (size - len(b))


def az(s, size):
    b = s.encode("ascii") + b"\0"
    assert len(b) <= size, s
    return b + b"\0" * (size - len(b))


CP950 = struct.pack("<I", 950)
CP932 = struct.pack("<I", 932)

SNEG_PATCHES = [
    # (ファイルオフセット, 元バイト, 新バイト, 説明)
    *[(text_off(va) + 1, CP950, CP932, "codepage 950->932 @%08x" % va) for va in (
        0x10046695, 0x10046925, 0x10046934, 0x1004696C,
        0x100AE515, 0x100AE58C, 0x100AE5BC, 0x100AE60B, 0x100AE661)],
    (text_off(0x100467B9) + 1, struct.pack("<I", 0x88), struct.pack("<I", 0x80),
     "GDI charset CHINESEBIG5 -> SHIFTJIS"),
    (text_off(0x100467CA) + 3, struct.pack("<I", 0x7D93), struct.pack("<I", 0x3042),
     "glyph probe U+7D93 -> U+3042"),
    (0x1A98B0, u16z("Microsoft JhengHei", 40), u16z(GDI_FACE, 40), "GDI face"),
    (0x1A98D8, u16z("Microsoft YaHei", 32), u16z(GDI_FALLBACK_FACE, 32), "GDI fallback face"),
]

DDRAW_PATCHES = [
    # コードページ 950 のときだけ CJK 文字一覧を読む・CJK フォント設定を有効にする判定 → 932
    *[(text_off(va) + 3, CP950, CP932, "codepage 950->932 @%08x" % va) for va in (
        0x1004B5D1, 0x1004BBC4, 0x10080917)],
    (0x14C060, az("languages/zh-hant/sneg_ui_cjk_characters.txt", 48),
     az(CJK_LIST, 48), "CJK char list path"),
    (text_off(0x1004BD79), bytes([0x6A, 44]), bytes([0x6A, len(CJK_LIST)]), "path length"),
    (0x14C090, az("msjh.ttc", 12), az(UI_FONT, 12), "UI font"),
    (text_off(0x1004B439), bytes([0x6A, 8]), bytes([0x6A, len(UI_FONT)]), "UI font length"),
    (0x14C09C, az("msyh.ttf", 12), az(UI_FONT_FALLBACK, 12), "UI fallback font"),
    (text_off(0x1004B500), bytes([0x6A, 8]), bytes([0x6A, len(UI_FONT_FALLBACK)]), "UI fallback length"),
]


def patch(src, dst, patches):
    d = bytearray(open(src, "rb").read())
    for off, old, new, desc in patches:
        assert len(old) == len(new), desc
        cur = bytes(d[off:off + len(old)])
        if cur == new:
            print("  済  ", desc)
            continue
        if cur != old:
            raise SystemExit("元バイト列が一致しません（%s @%#x）: %s" % (desc, off, cur.hex()))
        d[off:off + len(old)] = new
        print("  適用", desc)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    open(dst, "wb").write(bytes(d))


def main():
    game, out = sys.argv[1], sys.argv[2]
    for name, patches in (("sneg.dll", SNEG_PATCHES), ("ddraw.dll", DDRAW_PATCHES)):
        print(name)
        patch(os.path.join(game, name), os.path.join(out, name), patches)



# ---- ddraw.dll に直書きされた UI 文字列の日本語化（ddraw_text.py）
def _ddraw_text_patches():
    import ddraw_text as T
    here = os.path.dirname(os.path.abspath(__file__))
    d = open(os.path.join(here, "..", "backup", "ddraw.dll"), "rb").read()

    def span(off, en):  # 元の文字列 + 後ろの 0 埋め（NUL 1 つは残す）
        j = off + len(en)
        while d[j] == 0:
            j += 1
        return j - off

    def o2v(off):  # .rdata のファイルオフセット -> VA
        return off - 0x143600 + 0x10145000

    out = []
    for off, en, ja in T.TEXT:
        n = span(off, en)
        new = ja.encode("utf-8")
        assert len(new) < n, (en, ja)
        out.append((off, en.encode().ljust(n, b"\0"), new.ljust(n, b"\0"), "text " + en[:30]))
    # 置き場所（デバッグ文字列）を空けて訳を置く
    for a, en in T.MOVE_AREAS:
        n = span(a, en)
        buf = bytearray(n)
        for _, _, dst, ja in T.MOVE:
            if a <= dst < a + n:
                b = ja.encode("utf-8") + b"\0"
                assert dst - a + len(b) <= n, ja
                buf[dst - a:dst - a + len(b)] = b
        out.append((a, en.encode().ljust(n, b"\0"), bytes(buf), "area " + en[:20]))
    # 参照（push imm32 / mov eax, imm32）の付け替え
    for off, en, dst, ja in T.MOVE:
        old = struct.pack("<I", o2v(off))
        refs = [i for i in range(0x400, 0x143600 - 4) if d[i:i + 4] == old and d[i - 1] in (0x68, 0xB8)]
        assert len(refs) == 1, (en, refs)
        out.append((refs[0], old, struct.pack("<I", o2v(dst)), "ref %s -> %s" % (en, ja)))
    return out


DDRAW_PATCHES += _ddraw_text_patches()


if __name__ == "__main__":
    main()
