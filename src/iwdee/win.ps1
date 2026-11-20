param([int]$x=500,[int]$y=0,[int]$w=1296,[int]$h=759)
Add-Type @"
using System;using System.Runtime.InteropServices;
public class WW{[DllImport("user32.dll")]public static extern bool MoveWindow(IntPtr h,int x,int y,int w,int hh,bool r);
[DllImport("user32.dll")]public static extern bool ShowWindow(IntPtr h,int c);[DllImport("user32.dll")]public static extern bool SetForegroundWindow(IntPtr h);}
"@
$p=Get-Process icewind
[WW]::ShowWindow($p.MainWindowHandle,9) | Out-Null
[WW]::MoveWindow($p.MainWindowHandle,$x,$y,$w,$h,$true) | Out-Null
[WW]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
