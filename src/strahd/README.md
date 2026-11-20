# Ravenloft: Strahd's Possession 日本語化

DOSBox の `winmm.dll` として読み込まれ、文章ファイル T・CODE.1・CODE.2・OBJECTS.DAT・NPCS.DAT を日本語化したコピー（%LOCALAPPDATA%\RLOFT_JP）に差し替えるプログラムです。

## 組み立て方
1. ゲームのフォルダ（GAME と DOSBOX がある所）に作業フォルダ `_jp` を作り、ここの `src/` と `tools/` を置きます。
2. k8x12・k6x8 ゴシック・東雲 12 ドットなどの BDF フォントを `_jp/font/` に用意します。
3. 訳のファイル（`_jp/text/`）は含まれていません。`tools/` の抽出の道具で英文を書き出し、各自で訳を用意します。
4. `python tools/make_patch.py` で `build/patch.bin` を作り、`src/build.bat`（Visual Studio 2022 の 32 ビット cl）で `build/winmm.dll` を作ります。 `sh tools/rebuild.sh` でまとめて実行できます。
5. できた `winmm.dll` をゲームの `DOSBOX\` に置きます。

必要なもの: Python 3（Pillow）、Visual Studio 2022。原作のゲームファイルは各自で用意してください。

`tools/make_patch.py` は Dungeon Hack 日本語化（`src/dungeonhack/tools`）の make_patch.py を読み込みます。先頭の `DH` を置き場所に合わせて直してください。`tools/win.py`・`step.py` などは試験用の道具です。
