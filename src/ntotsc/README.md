# ノーザン・テイルズ・オブ・ザ・ソードコースト（NTotSC） 日本語版のソース

BG1 向けの WeiDU の MOD「Northern Tales of the Sword Coast」に日本語を足した版を組み立てる Python の道具です。MOD 本体は含みません。

## 組み立て
- 必要なもの: Python 3
- `python -X utf8 build_mods.py` で、MOD の配布 zip を展開し、英語の `.tra` と日本語の訳（`japanese/`・`japanese_parts/`）から `language/japanese` を作り、`.tp2` に Japanese の言語を足します。
- `validate.py` は訳の書式（記号・音声の指定・主人公の男女の言い分け）を確かめます。
- `fixtra.py`（`textfix.py` を使う）で、日本語の `.tra` の性別の書き分けを展開し、EE のエンジンで文章が欠ける半角空白を直します。
- `tra.py` は `.tra` を読む部品です。

## 含まれないもの（各自用意）
- MOD 本体の配布 zip、BGEE のゲームのファイル
- 訳のデータ（`japanese/`、`japanese_parts/`、`jobs.json`）
- `build_mods.py` の中の zip や出力先の場所は自分の環境に合わせて直してください。
