# アイ・オブ・ザ・ビホルダー II 日本語化パッチのソース

ゲームに付いている DOSBox の `DOSBOX` フォルダに置く `winmm.dll`（本物の winmm.dll へ呼び出しを渡す代理 DLL）のソースです。
DOSBox の中で動くゲームの文字の表示を横取りして日本語に置き換えます。

## 組み立て
- 必要なもの: Visual Studio 2022（x86 の C コンパイラ）、Python 3
- このフォルダの中身を、ゲームのフォルダの中の `_jp` フォルダとして置きます（道具は `../` にある原作のファイルを読みます）。
- `python -X utf8 tools/merge.py` で訳をまとめ、`python -X utf8 tools/make_patch.py` で `build/patch.bin` を作り、`src/build.bat` で DLL を作ります。
- `src/build.bat test`（`src/build_test.bat` も同じ）は試験用の DLL（`build/test/winmm.dll`、`DEVTEST` 付き。配布しない）、`src/build_trace.bat` は調べもの用の DLL です。`tools/win.py start jp` は試験用の DLL を使います。
- できあがり: `build/winmm.dll`

## 含まれないもの（各自用意）
- 原作のゲームのファイル
- 訳のデータ（`text/`）。このリポジトリには入っていません。
- フォントの BDF（`font/` に置く）: misaki_gothic_2nd.bdf（美咲ゴシック第2）
