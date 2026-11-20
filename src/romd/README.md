# Pool of Radiance: Ruins of Myth Drannor 日本語化

`wsock32.dll`（`loader/ja_loader.c`）がゲームの sneg.dll・ddraw.dll をメモリ上で書き換えて日本語（コードページ 932）を表示できるようにし、`languages/ja` の言語パックを使わせるプログラムです。本物の wsock32.dll へは全部の関数を中継します。

## 組み立て方
1. ゲームのフォルダに作業フォルダ `jp_mod` を作り、ここの `loader/` と `tools/` を置きます。元の sneg.dll・ddraw.dll を `jp_mod/backup/` に写します。
2. `python jp_mod/tools/extract.py .` で英文を書き出します。訳のファイル（`jp_mod/tr/`）は含まれていないので各自で用意します。
3. `python jp_mod/tools/build.py .` で言語パックと `loader/patches.h` を作り、`loader/build.bat`（Visual Studio 2022 の 32 ビット cl）で `wsock32.dll` を作ります。

必要なもの: Python 3（pefile）、Visual Studio 2022。原作のゲームファイルは各自で用意してください。
ddraw.dll の画面の文字列の訳（`tools/ddraw_text.py`）と訳語をそろえる表（`tools/harmonize.py`）は訳文の表なので含まれていません。build.py・patch_dlls.py は ddraw_text.py を読み込むので、同じ形の表を用意してください。
