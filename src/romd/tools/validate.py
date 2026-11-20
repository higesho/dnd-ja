"""翻訳結果（jp_mod/tr/*.json）をチャンク（jp_mod/chunks/*.json）と照合して検査する。

"keep": true の項目は原文のまま使う（検査しない）。
検査内容: id の過不足 / 未訳 / 書式記号（%s %d {x} \\n [X] 等）の一致 / cp932 で表せない文字 / タブ・改行混入
使い方: python validate.py [チャンク名 ...]   （省略時は tr にある全ファイル）
終了コード: 問題があれば 1
"""
import glob
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CH = os.path.join(HERE, "..", "chunks")
TR = os.path.join(HERE, "..", "tr")

TOKEN = re.compile(r"(?<!\d)%[-+ 0#]*\d*(?:\.\d+)?[sdiuxXcf%]|\{[A-Za-z_0-9]+\}|\[[A-Z]\]|\n")  # 数字直後の % (50% concealment 等) は書式記号ではない
NORMALIZE = {"〜": "～", "—": "―", "−": "－", "‐": "-", "‑": "-", " ": " "}


def tokens(s):
    return sorted(TOKEN.findall(s))


def check(name):
    errs = []
    src = json.load(open(os.path.join(CH, name), encoding="utf-8"))
    p = os.path.join(TR, name)
    if not os.path.exists(p):
        return ["訳ファイルなし"]
    try:
        tr = json.load(open(p, encoding="utf-8"))
    except Exception as e:  # noqa: BLE001
        return ["JSON 読み込み失敗: %s" % e]
    ja, keep = {}, set()
    for it in tr:
        if not isinstance(it, dict) or "id" not in it:
            errs.append("不正な要素: %r" % (it,))
            continue
        if it.get("keep"):  # 意図的に原文のまま（クレジット等）
            keep.add(it["id"])
        ja[it["id"]] = it.get("ja", "")
    for it in src:
        i, en = it["id"], it["en"]
        if i in keep:
            continue
        t = ja.get(i)
        if t is None:
            errs.append("%s: 訳が無い" % i)
            continue
        if not t.strip():
            errs.append("%s: 空の訳" % i)
            continue
        if tokens(en) != tokens(t):
            errs.append("%s: 書式記号が不一致 %s -> %s" % (i, tokens(en), tokens(t)))
        if "\t" in t or ("\r" in t):
            errs.append("%s: タブ/CR を含む" % i)
        if i.startswith("por:") and "\n" in t:
            errs.append("%s: 会話文に改行" % i)
        for c in t if not i.startswith("ui:") else ():  # UI 文字列は UTF-8 なので対象外
            try:
                NORMALIZE.get(c, c).encode("cp932")
            except UnicodeEncodeError:
                errs.append("%s: cp932 に無い文字 %r" % (i, c))
        if i.startswith("ui:ui.settings.language."):
            continue
        if re.search(r"[A-Za-z]{4,}", t) and re.fullmatch(r"[\x00-\x7f]*", t) and t == en:
            errs.append("%s: 英語のまま" % i)
    extra = set(ja) - {it["id"] for it in src}
    if extra:
        errs.append("余分な id %d 件（例 %s）" % (len(extra), sorted(extra)[:3]))
    return errs


def main():
    names = sys.argv[1:] or [os.path.basename(p) for p in sorted(glob.glob(os.path.join(TR, "*.json")))]
    bad = False
    for n in names:
        n = n if n.endswith(".json") else n + ".json"
        errs = check(n)
        print("%-22s %s" % (n, "OK" if not errs else "問題 %d 件" % len(errs)))
        for e in errs[:15]:
            print("    ", e)
        bad |= bool(errs)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
