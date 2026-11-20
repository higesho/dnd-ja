# アイスウィンド・デイル エンハンスド・エディション 日本語化修正パッチのソース

公式日本語の文章ファイル（`lang/ja_JP/dialog.tlk`・`dialogf.tlk`）を直したものを作る Python の道具です。

## 組み立て
- 必要なもの: Python 3（一部の調べもの用の道具は numpy・pefile・capstone も使います）
- `python -X utf8 build_tlk.py` で、公式の日本語 TLK（`orig/`）に訳の直し（`out/*.json`）・機械的な直し（`fixes.py`）・用語の言い換え（`restyle.py`）を当てて `build/` に TLK を書き出します。
- `python -X utf8 restyle_lua.py` で画面の文字列ファイル `L_ja_JP.LUA` に同じ用語の言い換えを当てます。
- `verify_lua.py`・`validate.py`・`wrapscan.py` などは確かめ用です。`biff.py`・`tlk.py` はゲームのデータを読む部品です。

## 含まれないもの（各自用意）
- 原作のゲームのファイル（公式の日本語 TLK・LUA を含む）
- 訳のデータ（`out/`、`canon.tsv`、`iwd2_rules.tsv`、`iwd2_map_names.tsv`、直した `L_ja_JP.LUA`）
- フォント（Mgen+ を `override/MEIRYO.ttf` として同梱しています）
- 道具の中のゲームのフォルダの場所（`biff.py` の `GAME` など）は自分の環境に合わせて直してください。
