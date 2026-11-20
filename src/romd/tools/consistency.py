"""用語集どおりに訳されているかを調べる。

glossary.md / glossary_terms.md の表（| 英語 | 訳 |）を読み、英語原文にその語を含むのに
訳文に指定の訳語が含まれていない項目を数える。違反の多い語から表示する。

使い方: python consistency.py [表示件数=60]
"""
import glob
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")


def load_terms():
    terms = []
    for name in ("glossary.md", "glossary_terms.md"):
        for line in open(os.path.join(ROOT, name), encoding="utf-8"):
            m = re.match(r"^\|\s*(.+?)\s*\|\s*(.+?)\s*\|\s*$", line)
            if not m or m.group(1) in ("英語", "---"):
                continue
            ens = [e.strip() for e in m.group(1).split(" / ")]
            jas = [j.strip() for j in m.group(2).split(" / ")]
            if len(ens) != len(jas):
                continue
            for e, j in zip(ens, jas):
                e = re.sub(r"[（(].*?[）)]", "", e).strip()
                j = re.sub(r"[（(].*?[）)]|※.*", "", j).strip()
                if len(e) < 4 or not j:
                    continue
                # 対訳表は一般語が多いので、大文字始まりの2語以上の名称だけを見る
                if name == "glossary_terms.md" and not re.fullmatch(r"[A-Z][\w'-]*(?: (?:of|the|de|[A-Z][\w'-]*))+", e):
                    continue
                # 能力値・技能などの一般語は文脈で訳し分けるので除外
                if name == "glossary.md" and e[0].islower():
                    continue
                terms.append((e, j))
    return terms


def main():
    limit = int(sys.argv[1]) if len(sys.argv) > 1 else 60
    en, ja = {}, {}
    for p in glob.glob(os.path.join(ROOT, "chunks", "*.json")):
        for it in json.load(open(p, encoding="utf-8")):
            en[it["id"]] = it["en"]
    for p in glob.glob(os.path.join(ROOT, "tr", "*.json")):
        for it in json.load(open(p, encoding="utf-8")):
            if not it.get("keep"):
                ja[it["id"]] = it["ja"]
    stats = []
    for e, j in dict.fromkeys(load_terms()):
        pat = re.compile(r"\b%s\b" % re.escape(e), re.I)
        hits = [i for i in ja if pat.search(en.get(i, ""))]
        bad = [i for i in hits if j not in ja[i]]
        if bad:
            stats.append((len(bad), len(hits), e, j, bad[:3]))
    stats.sort(reverse=True)
    for n, h, e, j, ex in stats[:limit]:
        print("%4d/%-4d %s → %s   例: %s" % (n, h, e, j, ", ".join(ex)))


if __name__ == "__main__":
    main()
