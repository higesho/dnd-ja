# バルダーズ・ゲート エンハンスド・エディション 日本語化修正パッチ（ソース）

公式の日本語訳の未訳・誤訳・データの不一致を直す WeiDU MOD（bgee_jpfix）と差し替え版の TLK を作るための Python スクリプトです。

- `sod/tlk.py` / `biff.py` / `mods/tra.py` … TLK・BIFF・WeiDU の tra の読み書き
- `sod/index.py` → `sod/todo.py` → `sod/bgee_audit*.py` … 公式訳を英語版と照らし、直す文章を洗い出す
- `sod/build_fix.py` → `sod/fixvalidate.py` / `sod/fixtra.py` / `sod/textfix.py` … 直す文章の一覧を作り、直した訳を確かめ整える
- `sod/build_weidu.py`（WeiDU MOD）、`sod/build_dropin.py`（差し替え版の TLK）、`sod/fixlist.py`（修正一覧）
- `sod/verify_install.py` / `sod/spacescan.py` … 組み込んだ結果を確かめる道具
- `build_weidu.py` が読む用語をそろえる表（final.py）は訳文を含むため入れていません

訳文・フォントのデータ・原作のファイルは含みません。原作は各自で用意してください。
