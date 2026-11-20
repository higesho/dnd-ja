# プールズ・オブ・ダークネス 日本語化パッチ（ソース）

DOSBox の `winmm.dll`（ゲームのフォルダの `DOSBOX\` に置く）として読み込まれ、ゲームが読むファイルと画面に出る文字を実行時に日本語へ差し替える DLL のソースです。

- `src/` … DLL の C ソースと組み立て用の `build.bat`（Visual Studio 2022 の x86 用 C コンパイラ）
- `tools/` … 訳の表から DLL に埋め込むデータを作る Python スクリプト、ゲームのデータを読む道具、試験用の道具
- 組み立て: ゲームのフォルダの下の `_jp` に置き、`sh tools/rebuild.sh` を実行（`tools/make_patch.py` でデータを作り、`src/build.bat` で DLL を組み立てる）。`build/winmm.dll` ができます

訳の表（text/）・フォントのデータ・原作のファイルは含みません。原作は各自で用意してください。フォントは BDF 形式のものを `font/` に置きます。
