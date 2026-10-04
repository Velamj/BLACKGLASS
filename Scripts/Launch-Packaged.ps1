param(
 [int]$Width = 1920,
 [int]$Height = 1080
)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$launch = Join-Path $taskRoot 'Artifacts\Windows\Blackglass.exe'
$game = Join-Path $taskRoot 'Artifacts\Windows\BLACKGLASS\Binaries\Win64\Blackglass.exe'
if (!(Test-Path -LiteralPath $launch) -or !(Test-Path -LiteralPath $game)) {
 throw 'Package BLACKGLASS with Build-Native.ps1 -Mode Package before launching.'
}
Start-Process -FilePath $launch -ArgumentList @(
 '-windowed', "-ResX=$Width", "-ResY=$Height", '-ForceRes', '-NoSplash'
) -WorkingDirectory (Split-Path -Parent $launch)
