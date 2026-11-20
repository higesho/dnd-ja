# レイヴンロフト ～ストーン・プロフェット～ 日本語化パッチのソース

ゲームに付いている DOSBox の `DOSBOX` フォルダに置く `winmm.dll`（本物の winmm.dll へ呼び出しを渡す代理 DLL）のソースです。
DOSBox の中で動くゲームの文字の表示を横取りして日本語に置き換えます。

## 組み立て
- 必要なもの: Visual Studio 2022（x86 の C コンパイラ）、Python 3
- このフォルダの中身を、ゲームのフォルダの中の `_jp` フォルダとして置きます（道具は `../` にある原作のファイルを読みます）。
- `sh tools/rebuild.sh` で、訳のまとめ → `build/patch.bin` → `src/build.bat` による DLL の組み立てを続けて行います。
- フォントの組み立てにダンジョン・ハックの `tools/make_patch.py`（`../dungeonhack/tools/`）を使います。`tools/make_patch.py` の `DH` を置き場所に合わせて直してください。
- `tools/code_patches.py` はゲームの実行ファイル（CODE.1 / CODE.2）への書き換えの一覧です。
- できあがり: `build/winmm.dll`

## 含まれないもの（各自用意）
- 原作のゲームのファイル
- 訳のデータ（`text/`）。このリポジトリには入っていません。
- フォントの BDF（`font/` に置く）: k8x12.bdf（k8x12）、k6x8_gothic.bdf ほか、ダンジョン・ハックと同じもの
