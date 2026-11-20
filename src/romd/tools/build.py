"""翻訳済み JSON から日本語化パック（jp_mod/dist）を組み立てる。

入力 : jp_mod/tr/**/*.json  … [{"id": ..., "ja": ...}, ...]（未訳の id は英語のまま）
出力 : jp_mod/dist/（install_ja.bat 等 + ja_files/ 以下がゲームフォルダに入る中身）
         languages/ja/ 一式（language.ini, sfDat.dat, sneg_ui_strings.dat,
                         sneg_ui_cjk_characters.txt, data/str, data/Rand/STR, data/Dialogue）
         wsock32.dll（日本語化ローダー。loader/ をビルドしたもの）
         と同名の zip

使い方: python build.py <ゲームフォルダ>
"""
import glob
import json
import os
import shutil
import sys
import unicodedata

sys.path.insert(0, os.path.dirname(__file__))
import porfmt  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
TR = os.environ.get("JP_TR") or os.path.join(HERE, "..", "tr")
RELEASE = "RuinsOfMythDrannor_JP"
DIST = os.path.join(HERE, "..", "dist", RELEASE)  # この中身をゲームのフォルダに置く
FILES = DIST
PACK = os.path.join(FILES, "languages", "ja")
KIT = os.path.join(HERE, "..", "kit")  # 説明書などの同梱物
LOADER = os.path.join(HERE, "..", "loader")

LANG_INI = """[Language]
CodePage=932
Release=Japanese fan translation
"""

# cp932 に無い・表示が崩れやすい文字の置き換え
NORMALIZE = {
    "〜": "～",  # 〜 -> ～
    "—": "―",  # — -> ―
    "−": "－",  # − -> －
    "‐": "-", "‑": "-",
    " ": " ",
    "…": "…",  # … はそのまま
    "⸺": "――",
}

problems = []


def load_tr():
    tr = {}
    for p in glob.glob(os.path.join(TR, "**", "*.json"), recursive=True):
        for it in json.load(open(p, encoding="utf-8")):
            ja = it.get("ja")
            if ja and not it.get("keep"):  # keep は原文のバイト列をそのまま使う
                tr[it["id"]] = ja
    return tr


def enc(ja, ident):
    s = "".join(NORMALIZE.get(c, c) for c in ja)
    s = unicodedata.normalize("NFC", s)
    out = bytearray()
    for c in s:
        try:
            out += c.encode("cp932")
        except UnicodeEncodeError:
            problems.append("%s: cp932 に無い文字 %r" % (ident, c))
            out += b"?"
    return bytes(out)


def main():
    game = sys.argv[1]
    j = lambda *p: os.path.join(game, *p)
    backup = os.path.join(HERE, "..", "backup")

    def orig(name):  # 導入済みの環境でも元ファイル（jp_mod/backup）を使う
        b = os.path.join(backup, name)
        return b if os.path.exists(b) else j(name)

    tr = load_tr()
    used = set()
    if os.path.isdir(DIST):
        shutil.rmtree(DIST)
    os.makedirs(PACK)
    for f in glob.glob(os.path.join(KIT, "*")):
        shutil.copy(f, DIST)
    open(os.path.join(PACK, "language.ini"), "w", encoding="ascii", newline="\r\n").write(LANG_INI)

    # sfDat
    sf = porfmt.read_sfdat(j("sfDat.dat"))
    out = []
    for i, s in enumerate(sf):
        ja = tr.get("sfdat:%d" % i)
        if ja is not None:
            used.update(ja)
            out.append(enc(ja, "sfdat:%d" % i))
        else:
            out.append(s)
    porfmt.write_sfdat(os.path.join(PACK, "sfDat.dat"), out)

    # UI（UTF-8 のまま）
    ver, _, items = porfmt.read_ui(orig("sneg_ui_strings.dat"))
    ui_items = []
    for k, cp, v in items:
        ja = tr.get("ui:" + k, v)
        used.update(ja)
        ui_items.append((k, cp, ja))
    if not any(k == "ui.settings.language.ja" for k, _, _ in ui_items):
        ui_items.append(("ui.settings.language.ja", 65001, "日本語"))
    porfmt.write_ui(os.path.join(PACK, "sneg_ui_strings.dat"), ver, "ja", ui_items)
    # 英語 UI 側にも言語名を追加（言語選択メニュー用）

    # .str
    for group, sub in (("str", ("data", "str")), ("rstr", ("data", "Rand", "STR"))):
        for p in sorted(glob.glob(j(*sub, "*.str"))):
            name = os.path.basename(p)
            lines, trailing = porfmt.read_lines(p)
            hit = False
            for i in range(len(lines)):
                ja = tr.get("%s:%s:%d" % (group, name, i))
                if ja is not None:
                    used.update(ja)
                    lines[i] = enc(ja, "%s:%s:%d" % (group, name, i))
                    hit = True
            if hit:
                d = os.path.join(PACK, *sub)
                os.makedirs(d, exist_ok=True)
                porfmt.write_lines(os.path.join(d, name), lines, trailing)

    # .por
    for p in sorted(glob.glob(j("data", "Dialogue", "*.por"))):
        name = os.path.basename(p)
        rows, trailing = porfmt.read_por(p)
        hit = False
        for i, r in enumerate(rows):
            ja = tr.get("por:%s:%d" % (name, i))
            if ja is not None and len(r) > 1:
                ja = ja.replace("\t", " ").replace("\r", "").replace("\n", " ")
                used.update(ja)
                r[1] = enc(ja, "por:%s:%d" % (name, i))
                hit = True
        if hit:
            for r in rows:  # 4 列目（英語の演技指示）は配布しない
                if len(r) > 3:
                    r[3] = b"null"
            d = os.path.join(PACK, "data", "Dialogue")
            os.makedirs(d, exist_ok=True)
            porfmt.write_por(os.path.join(d, name), rows, trailing)

    # ddraw.dll に直書きの UI 文字列（ddraw_text.py）の文字も含める
    import ddraw_text
    for _, _, ja in ddraw_text.TEXT:
        used.update(ja)
    for _, _, _, ja in ddraw_text.MOVE:
        used.update(ja)

    # UI 用の CJK 文字一覧（ImGui がこの文字のグリフを焼き込む）
    cjk = sorted(c for c in used if ord(c) >= 0x2000)
    open(os.path.join(PACK, "sneg_ui_cjk_characters.txt"), "w", encoding="utf-8").write("".join(cjk))

    # DLL パッチ
    # 日本語化ローダー wsock32.dll（sneg.dll / ddraw.dll をメモリ上で書き換える）をビルドして同梱
    import subprocess
    import gen_loader
    gen_loader.main()
    subprocess.run(["cmd", "/c", os.path.join(LOADER, "build.bat")], check=True, stdout=subprocess.DEVNULL)
    shutil.copy(os.path.join(LOADER, "wsock32.dll"), os.path.join(FILES, "wsock32.dll"))
    shutil.make_archive(DIST, "zip", os.path.dirname(DIST), RELEASE)

    print("翻訳適用 %d 件 / CJK 文字 %d 種" % (len(tr), len(cjk)))
    for p in problems[:50]:
        print("警告", p)
    if len(problems) > 50:
        print("警告 ほか %d 件" % (len(problems) - 50))


if __name__ == "__main__":
    main()
