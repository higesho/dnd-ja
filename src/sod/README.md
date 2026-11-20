# Siege of Dragonspear 日本語化

Baldur's Gate EE の日本語の会話ファイル（dialog.tlk）に Siege of Dragonspear の訳を入れる WeiDU の MOD と、差し替え版の dialog.tlk を作る道具です。

## 組み立て方
1. `tools/index.py` で英語・日本語の dialog.tlk を読み、`todo.py`・`build_batches.py` で訳す項目を書き出します。訳のファイルは含まれていないので各自で用意します（`validate.py` で検査）。
2. `build_tlk.py` で dialog.tlk を、`build_weidu.py` で WeiDU の MOD（`sod_ja.tp2` と tra）を、`build_dropin.py` で差し替え版を作ります。`textfix.py`・`fixtra.py` は文字の詰め方などを直します。
3. `install/かんたんインストール.bat` は配布物に入れるインストール用のバッチです（cp932）。

必要なもの: Python 3、WeiDU。原作のゲームファイル（BGEE と SoD）は各自で用意してください。各スクリプトの先頭のフォルダの場所は自分の環境に合わせて直してください。
build_weidu.py が読み込む訳語の統一表（final.py）は訳文の表なので含まれていません。
