param(
 [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
 [ValidateSet('Editor', 'Game', 'Package')][string]$Mode = 'Editor'
)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $taskRoot 'BLACKGLASS.uproject'
$build = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
if (!(Test-Path -LiteralPath $project) -or !(Test-Path -LiteralPath $build)) {
 throw 'BLACKGLASS project or Unreal build tool is missing.'
}
if (!$env:TMP) { $env:TMP = $env:TEMP }
if (!$env:ComSpec) { $env:ComSpec = Join-Path $env:SystemRoot 'System32\cmd.exe' }
if ($Mode -eq 'Package') {
 $uat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
 $archive = Join-Path $taskRoot 'Artifacts\Windows'
 & $uat BuildCookRun "-project=$project" -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive "-archivedirectory=$archive" -map=DepotBlock -utf8output
} else {
 $target = if ($Mode -eq 'Editor') { 'BlackglassEditor' } else { 'Blackglass' }
 & $build $target Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=4
}
if ($LASTEXITCODE -ne 0) { throw "Unreal build failed with exit code $LASTEXITCODE." }
