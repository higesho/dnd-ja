Get-Process icewind -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 3
Start-Process "steam://rungameid/321800"
for($i=0;$i -lt 60;$i++){
  Start-Sleep 1
  $p=Get-Process icewind -ErrorAction SilentlyContinue
  if($p -and $p.MainWindowHandle -ne 0){break}
}
Start-Sleep 4
& "$PSScriptRoot\win.ps1"
"started"
