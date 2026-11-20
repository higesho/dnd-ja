@echo off
cd /d "%~dp0"
echo ============================================================
echo  Siege of Dragonspear 日本語化 かんたんインストール
echo ============================================================
if not exist chitin.key (
  echo [エラー] このファイルを BGEE のフォルダ（Baldur.exe がある場所）に置いてから実行してください。
  pause & exit /b 1
)
if not exist weidu.conf (echo lang_dir = ja_JP> weidu.conf)
findstr /i "lang_dir" weidu.conf | findstr /i "ja_JP" >nul
if errorlevel 1 (
  echo [エラー] weidu.conf の言語が日本語（ja_JP）ではありません。ゲームの言語を日本語にしてから weidu.conf を削除し、もう一度実行してください。
  pause & exit /b 1
)
set MERGED=0
if exist WeiDU.log findstr /i "DLCMERGER" WeiDU.log >nul && set MERGED=1
if "%MERGED%"=="0" (
  if exist dlc\sod-dlc.zip goto merge
  if exist sod-dlc.zip goto merge
  echo [エラー] Siege of Dragonspear が見つかりません。
  pause & exit /b 1
)
goto fix
:merge
echo.
echo [1/3] DLC Merger で SoD を本体に統合します（数分かかります）...
.\setup-DlcMerger.exe --no-exit-pause --noautoupdate --language 0 --force-install 1
:fix
echo.
if exist setup-bgee_jpfix.exe (
  findstr /i "BGEE_JPFIX" WeiDU.log >nul 2>&1 || (
    echo [2/3] BGEE 公式日本語訳の修正MOD をインストールします...
    .\setup-bgee_jpfix.exe --no-exit-pause --noautoupdate --language 0 --force-install 0
  )
)
echo.
findstr /i "SOD_JA" WeiDU.log >nul 2>&1 && (echo SoD 日本語化はインストール済みです。& goto end)
echo [3/3] SoD 日本語化MOD をインストールします...
.\setup-sod_ja.exe --no-exit-pause --noautoupdate --language 0 --force-install 0
:end
echo.
echo 完了しました。WeiDU.log に SOD_JA の行があれば成功です。
pause
