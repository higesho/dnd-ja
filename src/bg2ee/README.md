# バルダーズ・ゲートII エンハンスド・エディション 日本語化パッチ（ソース）

ゲームの文章ファイル（lang/ja_JP の dialog.tlk / dialogF.tlk）を作るための Python スクリプトです。

- `tlk.py` / `biff.py` … TLK・BIFF の読み書き
- `index.py` … 対話・アイテム・呪文などから各文章の使われ方を調べる
- `build_batches.py` → `validate.py` → `build_tlk.py` … 訳していない文章を一覧にし、訳を確かめ、TLK を組み立てる
- `find.py` / `glos.py` / `glos2.py` … 既存の訳や用語を探す道具、`shot*.ps1` … 画面を撮る道具

訳文・フォントのデータ・原作のファイルは含みません。原作は各自で用意してください。
