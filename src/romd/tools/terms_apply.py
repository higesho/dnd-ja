"""用語表の作成・照合・自動修正。

  python terms_apply.py merge   … terms/survey_*.json と terms/overrides.json から
                                   terms/terms.json と 用語表.md を作る
  python terms_apply.py auto    … 英語原文にその用語を含む項目に限り、揺れた訳（renderings）を正しい訳に置換
  python terms_apply.py check   … 用語表どおりでない項目を terms/fix/<チャンク>.json に書き出し、件数を表示

overrides.json: {"英語": "訳"} で canonical を上書き、{"英語": null} で用語表から外す。
"""
import glob
import json
import os
import re
import sys
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
TDIR = os.path.join(ROOT, "terms")
TYPE_JA = {"person": "人名・神名", "place": "地名・場所", "group": "種族・組織", "item": "アイテム",
           "spell": "呪文・特技・能力", "creature": "モンスター", "rule": "ルール用語", "ui": "画面の用語",
           "other": "その他"}


def load_tr():
    en, ja, where = {}, {}, {}
    for p in glob.glob(os.path.join(ROOT, "chunks", "*.json")):
        for it in json.load(open(p, encoding="utf-8")):
            en[it["id"]] = it["en"]
    for p in glob.glob(os.path.join(ROOT, "tr", "*.json")):
        for it in json.load(open(p, encoding="utf-8")):
            if not it.get("keep"):
                ja[it["id"]] = it["ja"]
                where[it["id"]] = os.path.basename(p)
    return en, ja, where


def term_re(term):
    return re.compile(r"(?<![A-Za-z])%s(?![a-z])" % re.escape(term))


def merge():
    terms = {}
    for p in sorted(glob.glob(os.path.join(TDIR, "survey_*.json"))):
        for t in json.load(open(p, encoding="utf-8")):
            if t.get("type") == "skip" or not t.get("canonical"):
                continue
            terms[t["term"]] = {"term": t["term"], "type": t.get("type", "other"),
                                "canonical": t["canonical"].strip(),
                                "renderings": [r for r in t.get("renderings", []) if r],
                                "note": t.get("note", "")}
    ov_path = os.path.join(TDIR, "overrides.json")
    if os.path.exists(ov_path):
        for k, v in json.load(open(ov_path, encoding="utf-8")).items():
            if v is None:
                terms.pop(k, None)
            elif k in terms:
                terms[k]["canonical"] = v
            else:
                terms[k] = {"term": k, "type": "other", "canonical": v, "renderings": [], "note": "手動追加"}
    out = sorted(terms.values(), key=lambda t: (t["type"], t["term"].lower()))
    json.dump(out, open(os.path.join(TDIR, "terms.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    lines = ["# 用語表（Pool of Radiance: Ruins of Myth Drannor 日本語化）", "",
             "全訳の総見直しで決めた統一訳。翻訳・修正はこの表に従う。", ""]
    by = defaultdict(list)
    for t in out:
        by[t["type"]].append(t)
    for ty in TYPE_JA:
        if not by[ty]:
            continue
        lines += ["## " + TYPE_JA[ty], "", "| 英語 | 訳 |", "|---|---|"]
        lines += ["| %s | %s |" % (t["term"], t["canonical"]) for t in by[ty]]
        lines.append("")
    open(os.path.join(ROOT, "用語表.md"), "w", encoding="utf-8").write("\n".join(lines))
    print("用語表 %d 語" % len(out))


def load_terms():
    return json.load(open(os.path.join(TDIR, "terms.json"), encoding="utf-8"))


def auto():
    en, ja, where = load_tr()
    terms = load_terms()
    changed = defaultdict(dict)
    for t in terms:
        can = t["canonical"]
        # 置換して安全な揺れだけ: 正しい訳を含まない、3文字以上
        vars_ = [r for r in t["renderings"] if r != can and can not in r and r not in can and len(r) >= 3]
        if not vars_:
            continue
        pat = term_re(t["term"])
        for i, e in en.items():
            if i not in ja or not pat.search(e):
                continue
            s = changed[where[i]].get(i, ja[i])
            if can in s:
                continue
            for r in sorted(vars_, key=len, reverse=True):
                if r in s:
                    s = s.replace(r, can)
            if s != ja[i]:
                changed[where[i]][i] = s
    total = 0
    for fn, m in changed.items():
        p = os.path.join(ROOT, "tr", fn)
        data = json.load(open(p, encoding="utf-8"))
        for it in data:
            if it["id"] in m:
                it["ja"] = m[it["id"]]
        json.dump(data, open(p, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
        total += len(m)
    print("自動置換 %d 件" % total)


def check():
    en, ja, where = load_tr()
    terms = load_terms()
    flags = defaultdict(lambda: defaultdict(list))
    for t in terms:
        pat = term_re(t["term"])
        for i, e in en.items():
            if i in ja and pat.search(e) and t["canonical"] not in ja[i]:
                flags[where[i]][i].append({"term": t["term"], "canonical": t["canonical"]})
    fdir = os.path.join(TDIR, "fix")
    os.makedirs(fdir, exist_ok=True)
    for f in glob.glob(os.path.join(fdir, "*.json")):
        os.remove(f)
    total = 0
    for fn in sorted(flags):
        items = [{"id": i, "en": en[i], "ja": ja[i], "terms": ts} for i, ts in flags[fn].items()]
        json.dump(items, open(os.path.join(fdir, fn), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
        print("%-22s %4d 件" % (fn, len(items)))
        total += len(items)
    print("用語表と不一致 %d 件" % total)


if __name__ == "__main__":
    {"merge": merge, "auto": auto, "check": check}[sys.argv[1]]()
