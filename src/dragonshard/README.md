# ドラゴンシャード 日本語化パッチ（ソース）

ゲームのフォルダに置く dinput8.dll（システムの dinput8.dll の代理）のソースです。初回起動時に Text.H2O と Fonts.H2O を日本語入りで作り直して差し替え、折り返しと字の検索を日本語向けに直します。

- `src/` … DLL の C ソースと組み立て用の `build.bat`（Visual Studio 2022 の x86 用 C コンパイラ）
- `tools/` … H2O アーカイブを読む道具、訳の表から埋め込みデータとフォントを作る Python スクリプト、試験用の道具
- 組み立て: `sh tools/rebuild.sh`（`merge.py` → `make_data.py` → `make_font.py` → `src/build.bat`）。`build/dinput8.dll` ができます
- 配布版と試験版: `src/build.bat` は配布版（`build/dinput8.dll`）、`src/build.bat test` は試験版（`build/test/dinput8.dll`、`DSJP_TEST` 付きで `testio.c` 入り）を作ります。試験の道具（`tools/win.py`）は試験版を使います。試験版は配布しません
- `src/explode.c` と `tools/blast.py` は Mark Adler 氏の blast（zlib の contrib）を元に書き直したもので、この 2 つだけは zlib ライセンスです（冒頭に表示があります）

訳の表（text/）・フォントのデータ・原作のファイルは含みません。原作は各自で用意してください。
