[CmdletBinding()]
param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot "../Artifacts/Toolchain"),
    [switch]$RequireUnreal
)
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$blockers = [System.Collections.Generic.List[string]]::new()
$programFilesX86 = [Environment]::GetEnvironmentVariable("ProgramFiles(x86)")
$vswhere = Join-Path $programFilesX86 "Microsoft Visual Studio/Installer/vswhere.exe"
$visualStudio = @()
if (Test-Path -LiteralPath $vswhere) {
    $vsJson = (& $vswhere -products "*" -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json) -join [Environment]::NewLine
    if ($LASTEXITCODE -eq 0) { $visualStudio = @($vsJson | ConvertFrom-Json) }
}
$sdkRoot = Join-Path $programFilesX86 "Windows Kits/10/Include"
$sdkVersions = @()
if (Test-Path -LiteralPath $sdkRoot) {
    $sdkVersions = @(Get-ChildItem -LiteralPath $sdkRoot -Directory | Select-Object -ExpandProperty Name)
}
$cmakeVersion = $null
if (Get-Command cmake -ErrorAction SilentlyContinue) { $cmakeVersion = (& cmake --version | Select-Object -First 1) }
$candidates = [System.Collections.Generic.List[string]]::new()
if (-not [string]::IsNullOrWhiteSpace($env:BLACKGLASS_UE_ROOT)) { $candidates.Add($env:BLACKGLASS_UE_ROOT) }
$epicRoot = Join-Path $env:ProgramFiles "Epic Games"
if (Test-Path -LiteralPath $epicRoot) {
    foreach ($directory in Get-ChildItem -LiteralPath $epicRoot -Directory -Filter "UE_*") { $candidates.Add($directory.FullName) }
}
$buildsKey = "HKCU:\Software\Epic Games\Unreal Engine\Builds"
if (Test-Path $buildsKey) {
    foreach ($property in (Get-ItemProperty $buildsKey).PSObject.Properties) {
        if ($property.Name -notlike "PS*" -and $property.Value -is [string]) { $candidates.Add($property.Value) }
    }
}
$launcherKey = "HKLM:\SOFTWARE\EpicGames\Unreal Engine"
if (Test-Path $launcherKey) {
    foreach ($key in Get-ChildItem $launcherKey) {
        $item = Get-ItemProperty $key.PSPath
        $property = $item.PSObject.Properties["InstalledDirectory"]
        if ($property -and $property.Value) { $candidates.Add([string]$property.Value) }
    }
}
$unreal = @()
foreach ($candidate in ($candidates | Sort-Object -Unique)) {
    $versionPath = Join-Path $candidate "Engine/Build/Build.version"
    $editorPath = Join-Path $candidate "Engine/Binaries/Win64/UnrealEditor.exe"
    if (-not (Test-Path -LiteralPath $editorPath)) { $editorPath = Join-Path $candidate "Engine/Binaries/Win64/UE4Editor.exe" }
    $unreal += [ordered]@{
        root = $candidate
        version = $(if (Test-Path -LiteralPath $versionPath) { Get-Content -Raw -LiteralPath $versionPath | ConvertFrom-Json } else { $null })
        buildBatchAvailable = (Test-Path -LiteralPath (Join-Path $candidate "Engine/Build/BatchFiles/Build.bat"))
        automationToolAvailable = (Test-Path -LiteralPath (Join-Path $candidate "Engine/Build/BatchFiles/RunUAT.bat"))
        editorAvailable = (Test-Path -LiteralPath $editorPath)
    }
}
$usableUnreal = @($unreal | Where-Object { $_.version -and $_.buildBatchAvailable -and $_.automationToolAvailable -and $_.editorAvailable })
if ($visualStudio.Count -eq 0) { $blockers.Add("Missing Visual Studio C++ toolset.") }
if ($sdkVersions.Count -eq 0) { $blockers.Add("Missing Windows SDK.") }
if ($usableUnreal.Count -eq 0) {
    $blockers.Add("Missing a complete Unreal Engine installation: Engine/Build/Build.version, Engine/Build/BatchFiles/Build.bat, Engine/Build/BatchFiles/RunUAT.bat and the Windows editor executable. Set BLACKGLASS_UE_ROOT on an Unreal-equipped Windows runner.")
}
$os = Get-CimInstance Win32_OperatingSystem
$report = [ordered]@{
    schemaVersion = 1
    utc = [DateTime]::UtcNow.ToString("o")
    sourceCommit = $env:GITHUB_SHA
    runnerName = $env:RUNNER_NAME
    imageOS = $env:ImageOS
    imageVersion = $env:ImageVersion
    operatingSystem = $os.Caption
    operatingSystemVersion = $os.Version
    cpu = @(Get-CimInstance Win32_Processor | Select-Object Name, NumberOfCores, NumberOfLogicalProcessors)
    gpu = @(Get-CimInstance Win32_VideoController | Select-Object Name, DriverVersion)
    memoryGiB = [Math]::Round($os.TotalVisibleMemorySize / 1MB, 2)
    visualStudio = @($visualStudio | Select-Object displayName, installationVersion, installationPath)
    windowsSDKs = $sdkVersions
    cmake = $cmakeVersion
    unrealCandidates = $unreal
    completeUnrealInstallations = $usableUnreal.Count
    blockers = $blockers.ToArray()
    gameplayExecuted = $false
    packagingExecuted = $false
    performanceMeasured = $false
}
$reportPath = Join-Path $OutputDirectory "environment.json"
$report | ConvertTo-Json -Depth 12 | Set-Content -Encoding utf8 -LiteralPath $reportPath
# Machine evidence only: never dump process environment variables or credentials.
Write-Output ($report | ConvertTo-Json -Depth 12)
if ($env:GITHUB_STEP_SUMMARY) {
    @"
## BLACKGLASS environment inspection
- Source commit: $($env:GITHUB_SHA)
- Windows: $($os.Caption) $($os.Version)
- C++ toolsets: $($visualStudio.Count)
- Windows SDKs: $($sdkVersions -join ", ")
- CMake: $cmakeVersion
- Complete Unreal installations: $($usableUnreal.Count)
- Gameplay, packaging and performance: not executed.

$($blockers -join [Environment]::NewLine)
"@ | Add-Content -Encoding utf8 -LiteralPath $env:GITHUB_STEP_SUMMARY
}
if ($RequireUnreal -and $blockers.Count -gt 0) { throw "Unreal verification blocked. $($blockers -join ' ')" }
