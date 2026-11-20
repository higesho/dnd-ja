# デーモンストーン 日本語化パッチ（ソース）

ゲームのフォルダに置く dinput8.dll（システムの dinput8.dll の代理）のソースです。画面の文字・字幕のファイルを日本語にしたコピーに差し替え、フォントに日本語の字を足し、折り返しを日本語向けに直します。

- `src/` … DLL の C ソースと組み立て用の `build.bat`（Visual Studio 2022 の x86 用 C コンパイラ）
- `tools/` … 訳の表から埋め込みデータとフォントを作る Python スクリプト、試験用の道具
- 組み立て: `sh tools/rebuild.sh`（`make_data.py` → `make_font.py` → `src/build.bat`）。`build/dinput8.dll` ができます

訳の表（text/）・フォントのデータ・原作のファイルは含みません。原作は各自で用意してください。
