# The Temple of Elemental Evil 日本語化

TemplePlus（TPlite）で動く ToEE に、`DWrite.dll` として読み込まれ、日本語の文章を DirectWrite で描き、ゲームの文章ファイルを日本語のものに差し替えるプログラムです。

## 組み立て方
1. ゲームのフォルダに作業フォルダ `_jp` を作り、ここの `src/` と `tools/` を置きます。
2. 訳のファイル（`_jp/text/`）は含まれていません。`tools/extract.py` で英文を書き出し、各自で訳を用意します。
3. `tools/make_font.py` は Noto Sans JP / Noto Serif JP（可変フォント）から使う字だけのフォントを作ります。フォントは各自で用意してください。
4. `sh tools/rebuild.sh`（merge → make_data → make_font → `src/build.bat`）で `build/DWrite.dll` ができます。
5. `src/build.bat` は配布版（`build/DWrite.dll`）、`src/build.bat test` は試験版（`build/test/DWrite.dll`、`TJP_TEST` 付きで `testio.c` 入り）を作ります。`rebuild.sh` は両方を作ります。試験版は配布しません。

必要なもの: Python 3（fontTools、pefile、Pillow）、Visual Studio 2022（32 ビット cl）。原作のゲームファイルは各自で用意してください。
`tools/win.py`・`chargen.sh` は試験用の道具で、試験版の `DWrite.dll` を使います。
