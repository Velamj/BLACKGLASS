param(
 [Parameter(Mandatory=$true)][int]$GameProcessId,
 [ValidateSet('Capture','Key','Click')][string]$Action='Capture',
 [string]$Key='Space',
 [int]$X=0, [int]$Y=0,
 [ValidateSet('Left','Right')][string]$Button='Left',
 [switch]$Shift, [switch]$Control
)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$project=Join-Path $taskRoot 'BLACKGLASS.uproject'
$process=Get-CimInstance Win32_Process -Filter "ProcessId=$GameProcessId"
$packagedGame=Join-Path $taskRoot 'Artifacts\Windows\BLACKGLASS\Binaries\Win64\Blackglass.exe'
$isEditorGame=$process -and $process.Name -eq 'UnrealEditor.exe' -and
 $process.CommandLine.Contains($project) -and $process.CommandLine -match '(^|\s)-game(\s|$)'
$isPackagedGame=$process -and $process.Name -eq 'Blackglass.exe' -and
 $process.ExecutablePath -eq $packagedGame
if (!$isEditorGame -and !$isPackagedGame) {
 throw 'This helper accepts only the BLACKGLASS standalone or archived Windows game process.'
}
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class BGWindow {
 [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
 [StructLayout(LayoutKind.Sequential)] public struct Point { public int X,Y; }
 [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
 [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hwnd,out Rect rect);
 [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hwnd,ref Point point);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
 [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint from,uint to,bool attach);
 [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr hwnd);
 [DllImport("user32.dll")] public static extern IntPtr SetActiveWindow(IntPtr hwnd);
 [DllImport("user32.dll")] public static extern IntPtr SetFocus(IntPtr hwnd);
 [DllImport("user32.dll")] public static extern IntPtr GetAncestor(IntPtr hwnd,uint flags);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hwnd,int command);
 [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(Point point);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd,out uint pid);
 [DllImport("user32.dll")] public static extern void mouse_event(uint flags,uint x,uint y,uint data,UIntPtr extra);
 [DllImport("user32.dll")] public static extern void keybd_event(byte key,byte scan,uint flags,UIntPtr extra);
}
'@
[BGWindow]::SetThreadDpiAwarenessContext([IntPtr](-4))|Out-Null
$game=Get-Process -Id $GameProcessId
$window=$game.MainWindowHandle
if ($window -eq [IntPtr]::Zero) { throw 'BLACKGLASS has no game window yet.' }
[uint32]$windowOwner=0
$gameThread=[BGWindow]::GetWindowThreadProcessId($window,[ref]$windowOwner)
if ($windowOwner -ne $GameProcessId) { throw 'The target window is not owned by BLACKGLASS; no input sent.' }
$gameRoot=[BGWindow]::GetAncestor($window,2)
function Test-BlackglassForeground {
 $foreground=[BGWindow]::GetForegroundWindow()
 [uint32]$foregroundOwner=0
 [BGWindow]::GetWindowThreadProcessId($foreground,[ref]$foregroundOwner)|Out-Null
 return $foregroundOwner -eq $GameProcessId -and [BGWindow]::GetAncestor($foreground,2) -eq $gameRoot
}
for ($focusAttempt=0; $focusAttempt -lt 3 -and !(Test-BlackglassForeground); ++$focusAttempt) {
 if ($focusAttempt -eq 1) {
  [BGWindow]::ShowWindow($window,6)|Out-Null
  Start-Sleep -Milliseconds 80
 }
 [BGWindow]::ShowWindow($window,9)|Out-Null
 [uint32]$foregroundOwner=0
 $foregroundThread=[BGWindow]::GetWindowThreadProcessId([BGWindow]::GetForegroundWindow(),[ref]$foregroundOwner)
 $currentThread=[BGWindow]::GetCurrentThreadId()
 $attachedThreads=[Collections.Generic.List[uint32]]::new()
 try {
  foreach ($targetThread in @($foregroundThread,$gameThread) | Select-Object -Unique) {
   if ($targetThread -ne 0 -and $targetThread -ne $currentThread -and
       [BGWindow]::AttachThreadInput($currentThread,$targetThread,$true)) { $attachedThreads.Add($targetThread) }
  }
  [BGWindow]::BringWindowToTop($window)|Out-Null
  [BGWindow]::SetForegroundWindow($window)|Out-Null
  [BGWindow]::SetActiveWindow($window)|Out-Null
  [BGWindow]::SetFocus($window)|Out-Null
  Start-Sleep -Milliseconds 120
 } finally {
  foreach ($targetThread in $attachedThreads) { [BGWindow]::AttachThreadInput($currentThread,$targetThread,$false)|Out-Null }
 }
 Start-Sleep -Milliseconds 80
}
if (!(Test-BlackglassForeground)) {
 [uint32]$blockedOwner=0
 [BGWindow]::GetWindowThreadProcessId([BGWindow]::GetForegroundWindow(),[ref]$blockedOwner)|Out-Null
 throw "Could not focus BLACKGLASS (target PID $GameProcessId, foreground PID $blockedOwner); no input sent."
}

$rect=New-Object BGWindow+Rect
$origin=New-Object BGWindow+Point
[BGWindow]::GetClientRect($window,[ref]$rect)|Out-Null
[BGWindow]::ClientToScreen($window,[ref]$origin)|Out-Null
$width=$rect.Right-$rect.Left
$height=$rect.Bottom-$rect.Top
if ($Action -ne 'Click') { [BGWindow]::SetCursorPos($origin.X+[int]($width/2),$origin.Y+[int]($height/2))|Out-Null }
function PressKey([byte]$code) {
 [BGWindow]::keybd_event($code,0,0,[UIntPtr]::Zero)
 Start-Sleep -Milliseconds 80
 [BGWindow]::keybd_event($code,0,2,[UIntPtr]::Zero)
}
if ($Action -eq 'Capture') {
 $folder=Join-Path $taskRoot 'Saved\Verification'
 [IO.Directory]::CreateDirectory($folder)|Out-Null
 $path=Join-Path $folder ('Game-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.png')
 $bitmap=New-Object Drawing.Bitmap($width,$height)
 $graphics=[Drawing.Graphics]::FromImage($bitmap)
 try {
  $graphics.CopyFromScreen($origin.X,$origin.Y,0,0,$bitmap.Size)
  $bitmap.Save($path,[Drawing.Imaging.ImageFormat]::Png)
 } finally { $graphics.Dispose();$bitmap.Dispose() }
 Write-Output ($path+' '+$width+'x'+$height)
} elseif ($Action -eq 'Key') {
 $codes=@{Space=32;One=49;Two=50;Three=51;Four=52;F5=116;F9=120;F8=119;F7=118}
 if ($codes.ContainsKey($Key)) { $code=[byte]$codes[$Key] }
 elseif ($Key -match '^[A-Z]$') { $code=[byte][char]$Key }
 else { throw 'Unsupported test key.' }
 PressKey $code
 Write-Output ('Sent key '+$Key+' to BLACKGLASS')
} else {
 if ($X -lt 0 -or $Y -lt 0 -or $X -ge $width -or $Y -ge $height) { throw 'Click outside game client.' }
 $point=New-Object BGWindow+Point
 $point.X=$origin.X+$X;$point.Y=$origin.Y+$Y
 $hit=[BGWindow]::WindowFromPoint($point)
 [uint32]$owner=0
 [BGWindow]::GetWindowThreadProcessId($hit,[ref]$owner)|Out-Null
 if ($owner -ne $GameProcessId) { throw 'Click is covered by another process; no input sent.' }
 [BGWindow]::SetCursorPos($point.X,$point.Y)|Out-Null
 try {
  if ($Shift) { [BGWindow]::keybd_event(16,0,0,[UIntPtr]::Zero) }
  if ($Control) { [BGWindow]::keybd_event(17,0,0,[UIntPtr]::Zero) }
  $down=if ($Button -eq 'Left') {2} else {8}
  [BGWindow]::mouse_event($down,0,0,0,[UIntPtr]::Zero)
  Start-Sleep -Milliseconds 80
  [BGWindow]::mouse_event($down*2,0,0,0,[UIntPtr]::Zero)
 } finally {
  if ($Shift) { [BGWindow]::keybd_event(16,0,2,[UIntPtr]::Zero) }
  if ($Control) { [BGWindow]::keybd_event(17,0,2,[UIntPtr]::Zero) }
 }
 Write-Output ("Sent $Button click ($X,$Y) to BLACKGLASS")
}
