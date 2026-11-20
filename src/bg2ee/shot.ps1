param([string]$out)
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;using System.Runtime.InteropServices;
public class SW{[DllImport("user32.dll")]public static extern bool GetWindowRect(IntPtr h,out R r);public struct R{public int L,T,Rt,B;}}
"@
$h=(Get-Process Baldur).MainWindowHandle; $r=New-Object SW+R; [SW]::GetWindowRect($h,[ref]$r)|Out-Null
$w=$r.Rt-$r.L; $hh=$r.B-$r.T
$bmp=New-Object Drawing.Bitmap $w,$hh; $g=[Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($r.L,$r.T,0,0,$bmp.Size); $bmp.Save($out,[Drawing.Imaging.ImageFormat]::Png); "$w x $hh"
