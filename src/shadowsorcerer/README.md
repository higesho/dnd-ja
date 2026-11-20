# Shadow Sorcerer 日本語化

DOSBox の `winmm.dll` として読み込まれ、SHADOW.COM・GAME.EXE・TEXT.DAT を日本語化したコピーに差し替え、文字の描画を日本語に対応させるプログラムです。

## 組み立て方
1. ゲームのフォルダ（GAME と DOSBOX がある所）に作業フォルダ `_jp` を作り、ここの `src/` と `tools/` を置きます。
2. 字形のフォント（美咲フォントなど）を `_jp/font/` に用意します。
3. 訳のファイル（`_jp/text/`）は含まれていません。`tools/` の抽出の道具で英文を書き出し、各自で訳を用意します。
4. `python tools/make_patch.py` で `build/patch.bin` を作り、`src/build.bat`（Visual Studio 2022 の 32 ビット cl）で `build/winmm.dll` を作ります。 `sh tools/rebuild.sh` でまとめて実行できます。
5. できた `winmm.dll` をゲームの `DOSBOX\` に置きます。

必要なもの: Python 3（Pillow）、Visual Studio 2022。原作のゲームファイルは各自で用意してください。

`tools/make_patch.py` は同じリポジトリの Dungeon Hack 日本語化（`src/dungeonhack/tools`）の make_patch.py を読み込んで字形を作ります。置き場所に合わせて先頭の `DH` を直してください。
