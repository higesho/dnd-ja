# ファンタジー・エンパイア 日本語化パッチのソース

ゲームに付いている DOSBox の `DOSBOX` フォルダに置く `winmm.dll`（本物の winmm.dll へ呼び出しを渡す代理 DLL）のソースです。
DOSBox の中で動くゲームの文字の表示を横取りして日本語に置き換えます。

## 組み立て
- 必要なもの: Visual Studio 2022（x86 の C コンパイラ）、Python 3
- このフォルダの中身を、ゲームのフォルダの中の `_jp` フォルダとして置きます（道具は `../` にある原作のファイルを読みます）。
- `python -X utf8 tools/make_patch.py` で訳とフォントを `build/patch.bin` にまとめ、`src/build.bat` で DLL を作ります（`patch.bin` は DLL のリソースに入ります）。
- `src/testio.c` は試験用の入出力（画面の撮影・入力の差し込み）です。配布版には入りません。`src/build.bat test` で、これとログを入れた試験版 `build/test/winmm.dll` を作ります（`tools/win.py` が使います。配布しません）。
- `tools/to_game.sh` はできた DLL をゲームのフォルダに写します。
- できあがり: `build/winmm.dll`（配布版）

## 含まれないもの（各自用意）
- 原作のゲームのファイル
- 訳のデータ（`text/`）。このリポジトリには入っていません。
- フォントの BDF（`font/` に置く）: misaki_gothic_2nd.bdf（美咲ゴシック第2）
