"""翻訳用 JSON（jp_mod/src）を扱いやすい大きさのチャンクに分割する。

出力: jp_mod/chunks/<wave>_<名前>_NN.json  … [{"id", "en"}, ...]
  wave1 : sfdat / ui / monsters.str（用語の基準になるもの）
  wave2 : 残りの str / rstr / 会話 por
同じ会話ファイルは1チャンク内に収める（文脈を保つため）。

使い方: python chunk.py [目安語数=3500]
"""
import glob
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "src")
OUT = os.path.join(HERE, "..", "chunks")
LIMIT = int(sys.argv[1]) if len(sys.argv) > 1 else 3500


def words(items):
    return sum(len(it["en"].split()) for it in items)


def load(name):
    return json.load(open(os.path.join(SRC, name), encoding="utf-8"))


def split(groups, prefix):
    """groups: [(グループ名, items)] を順に詰めて LIMIT 語ごとに分割。グループ内は分けない（大きすぎる場合を除く）。"""
    chunks, cur = [], []
    for _, items in groups:
        if cur and words(cur) + words(items) > LIMIT:
            chunks.append(cur)
            cur = []
        if words(items) > LIMIT:  # 1グループが大きい場合は行で分割
            part = []
            for it in items:
                part.append(it)
                if words(part) >= LIMIT:
                    chunks.append(part)
                    part = []
            cur = part
        else:
            cur += items
    if cur:
        chunks.append(cur)
    names = []
    for i, c in enumerate(chunks):
        name = "%s_%02d.json" % (prefix, i + 1)
        json.dump(c, open(os.path.join(OUT, name), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
        names.append((name, len(c), words(c)))
    return names


def main():
    os.makedirs(OUT, exist_ok=True)
    for f in glob.glob(os.path.join(OUT, "*.json")):
        os.remove(f)
    strs = load("str.json")
    mon = [it for it in strs if it["id"].startswith("str:monsters.str:")]
    rest_str = [it for it in strs if not it["id"].startswith("str:monsters.str:")]
    res = []
    res += split([("sfdat", [it]) for it in load("sfdat.json")], "w1_sfdat")
    res += split([("ui", [it]) for it in load("ui.json")], "w1_ui")
    res += split([("mon", mon)], "w1_monsters")
    res += split([("str", rest_str), ("rstr", load("rstr.json"))], "w2_str")
    por = [(os.path.basename(p), load(os.path.join("por", os.path.basename(p))))
           for p in sorted(glob.glob(os.path.join(SRC, "por", "*.json")))]
    res += split(por, "w2_por")
    for r in res:
        print("%-22s %5d 件 %6d 語" % r)
    print("合計 %d チャンク" % len(res))


if __name__ == "__main__":
    main()
