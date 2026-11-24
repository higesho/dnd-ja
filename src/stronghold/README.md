# D&D Stronghold 日本語化

DOSBox の `winmm.dll` として読み込まれ、STRONG.EXE と STRONG.DAT を日本語化したコピー（%LOCALAPPDATA%\SG_JP）に差し替えるプログラムです。

## 組み立て方
1. ゲームのフォルダ（GAME と DOSBOX がある所）に作業フォルダ `_jp` を作り、ここの `src/` と `tools/` を置きます。
2. 美咲ゴシック第2（misaki_gothic_2nd.bdf）を `_jp/font/` に用意します。
3. 訳のファイル（`_jp/text/`）は含まれていません。`tools/` の抽出の道具で英文を書き出し、各自で訳を用意します。
4. `python tools/make_patch.py` で `build/patch.bin` を作り、`src/build.bat`（Visual Studio 2022 の 32 ビット cl）で配布版の `build/winmm.dll` を作ります。
5. できた `winmm.dll` をゲームの `DOSBOX\` に置きます。

必要なもの: Python 3（Pillow）、Visual Studio 2022。原作のゲームファイルは各自で用意してください。

`tools/to_game.sh`・`tools/soak.sh`・`tools/win.py` は試験用の道具です。これらは `src/build.bat test` で作る試験版 `build/test/winmm.dll`（`src/testio.c` の画面の撮影・入力の差し込みとログが入る。配布しない）を使います。
