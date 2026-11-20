"""英語版のテキストを翻訳用 JSON に抽出する。

出力: jp_mod/src/<グループ>.json  … [{"id": ..., "en": ...}, ...]
  ui      : sneg_ui_strings.dat（キー単位）
  sfdat   : sfDat.dat（番号単位）
  str     : data/str/*.str（行単位）
  rstr    : data/Rand/STR/*.str（行単位）
  por/xxx : data/Dialogue/*.por の台詞列（行単位, 全 null のファイルは除外）

使い方: python extract.py <ゲームフォルダ>
"""
import glob
import json
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import porfmt  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "src")


def dec(b):
    try:
        return b.decode("cp1252")
    except UnicodeDecodeError:
        return b.decode("latin-1")


def skip(text):
    t = text.strip()
    return t == "" or t == "null"


def dump(name, items):
    path = os.path.join(SRC, name + ".json")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(items, f, ensure_ascii=False, indent=1)
    return len(items)


def main():
    game = sys.argv[1]
    j = lambda *p: os.path.join(game, *p)
    total = {}

    _, _, ui = porfmt.read_ui(j("sneg_ui_strings.dat"))
    total["ui"] = dump("ui", [{"id": "ui:" + k, "en": v} for k, _, v in ui if not skip(v)])

    sf = porfmt.read_sfdat(j("sfDat.dat"))
    total["sfdat"] = dump("sfdat", [{"id": "sfdat:%d" % i, "en": dec(s)}
                                    for i, s in enumerate(sf) if not skip(dec(s))])

    for group, pattern in (("str", j("data", "str", "*.str")), ("rstr", j("data", "Rand", "STR", "*.str"))):
        items = []
        for p in sorted(glob.glob(pattern)):
            name = os.path.basename(p)
            lines, _ = porfmt.read_lines(p)
            for i, l in enumerate(lines):
                if not skip(dec(l)):
                    items.append({"id": "%s:%s:%d" % (group, name, i), "en": dec(l)})
        total[group] = dump(group, items)

    n = 0
    for p in sorted(glob.glob(j("data", "Dialogue", "*.por"))):
        name = os.path.basename(p)
        rows, _ = porfmt.read_por(p)
        items = [{"id": "por:%s:%d" % (name, i), "en": dec(r[1])}
                 for i, r in enumerate(rows) if len(r) > 1 and not skip(dec(r[1]))]
        if items:
            dump("por/" + name[:-4], items)
            n += len(items)
    total["por"] = n
    print(total)


if __name__ == "__main__":
    main()
