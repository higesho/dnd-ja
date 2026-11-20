param([string]$out)
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;using System.Runtime.InteropServices;
public class PW{[DllImport("user32.dll")]public static extern bool PrintWindow(IntPtr h,IntPtr dc,uint f);[DllImport("user32.dll")]public static extern bool GetClientRect(IntPtr h,out R r);public struct R{public int L,T,Rt,B;}}
"@
$h=(Get-Process Baldur).MainWindowHandle; $r=New-Object PW+R; [PW]::GetClientRect($h,[ref]$r)|Out-Null
$bmp=New-Object Drawing.Bitmap ($r.Rt),($r.B); $g=[Drawing.Graphics]::FromImage($bmp); $dc=$g.GetHdc(); [PW]::PrintWindow($h,$dc,2)|Out-Null; $g.ReleaseHdc($dc); $bmp.Save($out,[Drawing.Imaging.ImageFormat]::Png); "$($r.Rt) x $($r.B)"
