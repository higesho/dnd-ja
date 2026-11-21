# アイ・オブ・ザ・ビホルダー 日本語化パッチのソース

ゲームに付いている DOSBox の `DOSBOX` フォルダに置く `winmm.dll`（本物の winmm.dll へ呼び出しを渡す代理 DLL）のソースです。
DOSBox の中で動くゲームの文字の表示を横取りして日本語に置き換えます。

## 組み立て
- 必要なもの: Visual Studio 2022（x86 の C コンパイラ）、Python 3
- このフォルダの中身を、ゲームのフォルダの中の `_jp` フォルダとして置きます（道具は `../` にある原作のファイルを読みます）。
- `python -X utf8 tools/make_patch.py` で訳とフォントを `build/patch.bin` にまとめ、`src/build.bat` で DLL を作ります（`patch.bin` は DLL のリソースに入ります）。
- `tools/intro_text.py` はオープニングの字幕画像を日本語で描き直します。
- `src/build_trace.bat` は調べもの用の DLL です（配布物には使いません）。
- `tools/win.py`・`rpm.py`・`poke.py`・`npc.sh`・`sheet.py` は動作を確かめるための道具です（DOSBox の起動と操作、画面の取り込み、人物との会話の場面の呼び出し）。
- できあがり: `build/winmm.dll`

## 含まれないもの（各自用意）
- 原作のゲームのファイル
- 訳のデータ（`text/`）。このリポジトリには入っていません。
- フォントの BDF（`font/` に置く）: misaki_gothic_2nd.bdf（美咲ゴシック第2）、shnm7x14a.bdf / shnmk14.bdf（東雲フォント 14 ドット）
