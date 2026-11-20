"""英語原文から用語候補（固有名詞・名称）を抽出し、各用語の用例（英文と訳文の組）を集める。

出力: jp_mod/terms/candidates.json
  [{"term": "Myth Drannor", "count": 129, "samples": [{"id", "en", "ja"}, ...]}, ...]
候補の条件:
  - 大文字始まりの語の連なり（of / the / 's を挟んでよい）で、文頭以外に2回以上現れるもの
  - glossary.md に載っている英語
使い方: python terms_extract.py [用例数=6]
"""
import glob
import json
import os
import re
import sys
from collections import Counter, defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
NSAMPLE = int(sys.argv[1]) if len(sys.argv) > 1 else 6

# 固有名詞にならない大文字語（文頭・一般語）
STOP = set("""
I I'm I'll I've I'd A An The This That These Those It Its It's If And But Or So Yes No Not Now Then There Here
You Your You're We We're Our They Their He His She Her Him Me My What Who Why How When Where Which Well Oh Ah
Do Don't Does Did Can Can't Will Would Should Could Must May Might Is Are Was Were Be Been Have Has Had Let Let's
All Some Any Each Every One Two Three Four Five First Second Third Last Next Only Even Still Just Also Very Too
For From With Without Into Onto Upon At In On Of To By As Up Down Out Over Under After Before Again Once
Please Thank Thanks Good Great Perhaps Maybe Indeed Aye Nay Hmm Ha Hah Alas Farewell Hello Greetings
Level Levels Note DM VOICE NAME OK Yes
""".split())

CAP = r"[A-Z][a-zA-Z'\-]*"
TERM = re.compile(r"%s(?:\s+(?:of|the|of the|de|du)\s+%s|\s+%s){0,4}" % (CAP, CAP, CAP))


def strip_poss(t):
    return re.sub(r"'s?$", "", t)


def main():
    en, ja = {}, {}
    for p in glob.glob(os.path.join(ROOT, "chunks", "*.json")):
        for it in json.load(open(p, encoding="utf-8")):
            en[it["id"]] = it["en"]
    for p in glob.glob(os.path.join(ROOT, "tr", "*.json")):
        for it in json.load(open(p, encoding="utf-8")):
            if not it.get("keep"):
                ja[it["id"]] = it["ja"]

    cnt = Counter()
    where = defaultdict(list)
    for i, t in en.items():
        if i not in ja:
            continue
        for m in TERM.finditer(t):
            term = strip_poss(m.group().strip())
            words = term.split()
            # 文頭の一般語を落とす（"The Weave" → "Weave" など）
            while words and words[0] in STOP:
                words = words[1:]
            if not words:
                continue
            term = " ".join(words)
            if len(term) < 3 or term in STOP or term.isupper() and len(term) < 5:
                continue
            # 文頭に1語だけの普通の語は除外（同じ語が文中で大文字になっていれば残る）
            start = m.start() == 0 or t[max(0, m.start() - 2):m.start()].strip() in (".", "!", "?", '"', "")
            if start and len(words) == 1:
                continue
            cnt[term] += 1
            where[term].append(i)

    # glossary.md の英語も必ず含める
    for line in open(os.path.join(ROOT, "glossary.md"), encoding="utf-8"):
        m = re.match(r"^\|\s*(.+?)\s*\|\s*(.+?)\s*\|\s*$", line)
        if not m or m.group(1) in ("英語", "---"):
            continue
        for e in m.group(1).split(" / "):
            e = re.sub(r"[（(].*?[）)]", "", e).strip()
            if e and e not in cnt:
                ids = [i for i, t in en.items() if i in ja and e in t]
                if ids:
                    cnt[e] = len(ids)
                    where[e] = ids

    out = []
    for term, c in cnt.most_common():
        if c < 2:
            continue
        ids = list(dict.fromkeys(where[term]))
        # 用例は id の種類（por/str/sfdat/ui）が散らばるように選ぶ
        ids.sort(key=lambda i: (i.split(":")[0], len(en[i])))
        step = max(1, len(ids) // NSAMPLE)
        pick = ids[::step][:NSAMPLE]
        out.append({"term": term, "count": c,
                    "samples": [{"id": i, "en": en[i][:300], "ja": ja[i][:300]} for i in pick]})
    os.makedirs(os.path.join(ROOT, "terms"), exist_ok=True)
    json.dump(out, open(os.path.join(ROOT, "terms", "candidates.json"), "w", encoding="utf-8"),
              ensure_ascii=False, indent=1)
    print("候補 %d 語（出現2回以上）" % len(out))


if __name__ == "__main__":
    main()
