param(
 [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
 [int]$Width = 1920,
 [int]$Height = 1080
)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $taskRoot 'BLACKGLASS.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$map = Join-Path $taskRoot 'Content\Maps\DepotBlock.umap'
if (!(Test-Path -LiteralPath $editor) -or !(Test-Path -LiteralPath $map)) {
 throw 'Build BlackglassEditor and create DepotBlock before launching.'
}
Start-Process -FilePath $editor -ArgumentList @(
 ('"' + $project + '"'), '/Game/Maps/DepotBlock', '-game',
 '-windowed', "-ResX=$Width", "-ResY=$Height", '-ForceRes', '-NoSplash'
) -WorkingDirectory $taskRoot
