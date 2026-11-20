# ダンジョン・ハック 日本語化パッチのソース

ゲームに付いている DOSBox の `DOSBOX` フォルダに置く `winmm.dll`（本物の winmm.dll へ呼び出しを渡す代理 DLL）のソースです。
DOSBox の中で動くゲームの文字の表示を横取りして日本語に置き換えます。

## 組み立て
- 必要なもの: Visual Studio 2022（x86 の C コンパイラ）、Python 3
- このフォルダの中身を、ゲームのフォルダの中の `_jp` フォルダとして置きます（道具は `../` にある原作のファイルを読みます）。
- `sh tools/rebuild.sh` で、訳のまとめ → `build/patch.bin` → `src/build.bat` による DLL の組み立てを続けて行います。
- できあがり: `build/winmm.dll`

## 含まれないもの（各自用意）
- 原作のゲームのファイル
- 訳のデータ（`text/`）。このリポジトリには入っていません。
- フォントの BDF（`font/` に置く）: misaki_gothic_2nd.bdf（美咲ゴシック第2）、shnmk14.bdf（東雲フォント 14 ドット）
