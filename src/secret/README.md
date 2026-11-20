# Secret of the Silver Blades 日本語化

DOSBox の `winmm.dll` として読み込まれ、ゲームが開くファイルを日本語化したものに差し替え、文字の描画を日本語に対応させるプログラムです。

## 組み立て方
1. ゲームのフォルダ（GAME と DOSBOX がある所）に作業フォルダ `_jp` を作り、ここの `src/` と `tools/` を置きます。
2. 美咲ゴシック第2（misaki_gothic_2nd.bdf）を `_jp/font/` に用意します。
3. 訳のファイル（`_jp/text/`）は含まれていません。`tools/` の抽出の道具で英文を書き出し、各自で訳を用意します。
4. `python tools/make_patch.py` で `build/patch.bin` を作り、`src/build.bat`（Visual Studio 2022 の 32 ビット cl）で `build/winmm.dll` を作ります。 `sh tools/rebuild.sh` でまとめて実行できます。
5. できた `winmm.dll` をゲームの `DOSBOX\` に置きます。

必要なもの: Python 3（Pillow）、Visual Studio 2022。原作のゲームファイルは各自で用意してください。

作品ごとの値（描画関数の位置など）は `game.json` にあります。`tools/win.py` などは試験用の道具です。
