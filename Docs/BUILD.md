# Build and verification instructions

BLACKGLASS has a native Unreal C++ project and a real foundation map. The pre-visual Package04 Windows development build completed its escort mission through ordinary controls on the verified toolchain. The subsequent visual Package05 has built, passed archived content/configuration checks, and received a running palette/HUD and ordinary-input review at a measured 1920x1080. Its bounded packaged performance capture also passed; full-mission and heavy-combat performance remain unmeasured. Build, runtime, input and presentation evidence are recorded separately in [VERIFICATION.md](VERIFICATION.md); a successful UnrealBuildTool run alone does not establish a playable milestone.

## Native project and dependencies

- Project: `BLACKGLASS.uproject`, associated with Unreal Engine 5.8.
- Runtime module: `Source/Blackglass/`; editor and game targets are `BlackglassEditor` and `Blackglass`.
- Foundation map: `Content/Maps/DepotBlock.umap` (`/Game/Maps/DepotBlock`).
- Runtime weapon definitions: `Content/Data/Weapons.csv`; range and hearing distances are authored in metres and converted to Unreal centimetres.
- Editor bridge: `Plugins/UE_MCP_Bridge/`, pinned to upstream ue-mcp 1.3.9. It is development tooling, not gameplay.
- Controls: [CONTROLS.md](CONTROLS.md).
- Build outputs and reports: `Binaries/`, `Intermediate/`, `Saved/` and `Artifacts/` are local generated files.

The verified development installation is **Unreal Engine 5.8.3**, with MSVC toolset **14.50.35728**, Windows SDK **10.0.26100**, and the engine's bundled **.NET 10**. These are observed versions, not a promise of compatibility with every other toolchain. Inspect any different installation before building; do not migrate projects or upgrade the engine implicitly.

The examples below use `<ProjectRoot>` to mean your BLACKGLASS checkout. Set `$engineRoot` to your actual installed engine root. The ordinary Epic installation path is shown as a replaceable example.

~~~powershell
Set-Location '<ProjectRoot>'
$projectRoot = (Get-Location).Path
$engineRoot = 'C:\Program Files\Epic Games\UE_5.8'
~~~

Close the BLACKGLASS editor before rebuilding its native module or bridge. Leave other projects and their editor sessions untouched.

## Inspect the installed engine

~~~powershell
$env:BLACKGLASS_UE_ROOT = $engineRoot
powershell -ExecutionPolicy Bypass -File .\Scripts\Inspect-Environment.ps1 -RequireUnreal
~~~

The inspection script checks the configured root, standard Epic installation locations and registered engine paths. A valid installation contains `Engine/Build/Build.version`, `Build.bat`, `RunUAT.bat` and the Windows editor executable. Its report is local at `Artifacts/Toolchain/environment.json`; inspect it before choosing version-specific APIs. Do not upload private machine reports or launcher logs as public evidence.

Install the locked editor tooling when working with the bridge:

~~~powershell
npm.cmd ci --ignore-scripts --no-audit --no-fund
~~~

Node 20 or newer is required by ue-mcp 1.3.9. Running npm successfully does not establish editor access. [UNREAL_MCP.md](UNREAL_MCP.md) describes project-bound connection checks; preserve the pinned version and avoid redeploying or updating the bridge during unrelated builds.

## Build the native editor target

~~~powershell
powershell -ExecutionPolicy Bypass -File .\Scripts\Build-Native.ps1 -EngineRoot $engineRoot -Mode Editor
~~~

The script invokes Unreal's `Build.bat BlackglassEditor Win64 Development` with the actual project path, disables hot-reload builds, and limits parallel build actions to four. A nonzero exit is reported as a failure. The module output is normally `Binaries/Win64/UnrealEditor-Blackglass.dll`; rely on the actual build log.

For the standalone game target without cooking or packaging:

~~~powershell
powershell -ExecutionPolicy Bypass -File .\Scripts\Build-Native.ps1 -EngineRoot $engineRoot -Mode Game
~~~

Building this target does not produce a self-contained packaged game.

## Create the foundation map only when it is missing

The map is already present in the working project. This command is for a fresh checkout that does not contain `Content/Maps/DepotBlock.umap`. The script deliberately refuses to overwrite an existing map.

~~~powershell
$editorCmd = Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $projectRoot 'BLACKGLASS.uproject'
$createScript = Join-Path $projectRoot 'Scripts\Create-Foundation.py'
& $editorCmd $project -run=pythonscript "-script=$createScript" -unattended -NullRHI -NoSound -NoSplash
if ($LASTEXITCODE -ne 0) { throw 'Foundation map creation failed.' }
~~~

`Create-Foundation.py` loads the compiled `BGOperation` class, creates a blank real Unreal map, assigns the game mode and saves `/Game/Maps/DepotBlock`. The game mode constructs the foundation district and authoritative gameplay actors when play begins; the map does not contain duplicate preplaced operatives.

## Regenerate the original foundation surface art

`Scripts/Create-Foundation-Materials.py` generates original, seeded 512x512 tileable concrete and asphalt TGA source images and builds the owned Unreal surface material. It downloads no artwork. Close the BLACKGLASS editor before regenerating assets; preserve other projects and their content.

~~~powershell
$editorCmd = Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $projectRoot 'BLACKGLASS.uproject'
$materialScript = Join-Path $projectRoot 'Scripts\Create-Foundation-Materials.py'
& $editorCmd $project -run=pythonscript "-script=$materialScript" -unattended -NoSound -NoSplash
if ($LASTEXITCODE -ne 0) { throw 'Foundation material generation failed.' }
$materialReport = Get-Content -LiteralPath .\Saved\Verification\material-generation.json -Raw | ConvertFrom-Json
if (!$materialReport.validated -or $materialReport.errors.Count -ne 0) {
    throw 'Foundation material validation failed; inspect the local report.'
}
~~~

Outputs are:

- Source images: `SourceArt/Materials/T_BG_ConcreteDetail.tga` (seed 1337) and `T_BG_AsphaltDetail.tga` (seed 8701).
- Imported textures: `/Game/Textures/T_BG_ConcreteDetail` and `/Game/Textures/T_BG_AsphaltDetail`.
- Surface material: `/Game/Materials/M_BlackglassSurface`, with Color, Roughness, SurfaceDetail, DetailScale and DetailStrength parameters.
- Local validation report: `Saved/Verification/material-generation.json`.

The script checks ownership metadata before replacing assets at those names and refuses unrelated existing assets. TextureFactory import validation checks saved assets for private machine import paths. A successful report establishes generation and material validation; inspect the running district and recook the package to verify its appearance. These authored surfaces improve the prototype and do not make its geometry, characters or complete art direction production-ready. See [ASSET_MANIFEST.md](ASSET_MANIFEST.md) for source and placeholder status.

## Launch through ordinary controls

~~~powershell
powershell -ExecutionPolicy Bypass -File .\Scripts\Launch-Foundation.ps1 -EngineRoot $engineRoot -Width 1920 -Height 1080
~~~

This opens `UnrealEditor.exe BLACKGLASS.uproject /Game/Maps/DepotBlock -game` as a windowed standalone game using the installed editor. It checks for the editor and map before launching. This is a real game window, but it is not packaged-executable evidence.

You can also open `BLACKGLASS.uproject` in the verified engine, load `DepotBlock`, and use Play. Four operatives are selected and the camera recenters once at startup. Read [CONTROLS.md](CONTROLS.md) for mouse and keyboard controls.

The foundation objective is to acquire the research specialist, escort them to the marked extraction area, and resolve the operation. This is an escort implementation; it does not implement the later neural-override system or campaign vertical slice. Primitive environment and character art remain placeholders.

## Optional Windows input and capture helper

For repeatable ordinary-input checks, `Scripts/Local-Game-Window.ps1` targets the BLACKGLASS standalone `UnrealEditor.exe -game` process or the exact archived native `Blackglass.exe` path. Supply that specific running process ID; the helper rejects unrelated editor sessions and refuses clicks covered by another process.

~~~powershell
$gameProcessId = 12345 # Replace with the actual BLACKGLASS standalone process ID.
powershell -ExecutionPolicy Bypass -File .\Scripts\Local-Game-Window.ps1 -GameProcessId $gameProcessId -Action Capture
powershell -ExecutionPolicy Bypass -File .\Scripts\Local-Game-Window.ps1 -GameProcessId $gameProcessId -Action Key -Key One
powershell -ExecutionPolicy Bypass -File .\Scripts\Local-Game-Window.ps1 -GameProcessId $gameProcessId -Action Key -Key P
~~~

Capture saves only the game client area under `Saved/Verification/Game-<timestamp>.png` and prints its actual pixel dimensions. Keep the game visible and unobscured. Screenshots remain private local artifacts unless separately approved for publication.

`-Action Click` accepts client-area pixel coordinates with `-X`, `-Y`, `-Button Left|Right`, and optional `-Shift` or `-Control`. Its `Key` action supports Space, One-Four, F5/F7/F8/F9 and single uppercase letter keys. This helper does not implement drag selection, mouse-wheel input or held-key movement; check those through ordinary controls.

Sending a key or producing a screenshot does not establish that the intended gameplay action passed. Inspect the running result, compare state before and after where appropriate, and record the actual outcome in [VERIFICATION.md](VERIFICATION.md). Record each graphical check separately; the completed Package04 mission and its F5/F9 regression verify the recorded actions, rather than every binding or subsequent visual build.


## Run native runtime automation

~~~powershell
python .\Scripts\Verify-Foundation.py --engine-root "$engineRoot" --run-label Foundation --timeout 300
~~~

Optional `--project` accepts an explicit `.uproject` path; the default is this checkout's `BLACKGLASS.uproject`. `--run-label` allows 1-64 letters, digits, underscores or hyphens. `--timeout` is 30-1800 seconds.

The runner starts the actual compiled Unreal game with `-NullRHI -NoSound`, runs `Blackglass.Foundation.Runtime`, and requires a successful engine Automation report as well as exit code zero. A zero process exit alone is insufficient. A timed-out process started by the runner is terminated and the run fails.

Reports remain local:

- `Saved/Automation/<RunLabel>-<UTC timestamp>/index.json`.
- `Saved/Verification/<RunLabel>-<UTC timestamp>.summary.json`.
- Separate stdout and engine logs under `Saved/Verification/`.

This verifies the native runtime systems exercised by the test. It does not verify graphics, audio, HUD legibility, ordinary input, 60 FPS or packaged compatibility. Use the recorded actual results in [VERIFICATION.md](VERIFICATION.md); failed test runs remain failures until corrected and rerun.

## Package a Windows development build

~~~powershell
powershell -ExecutionPolicy Bypass -File .\Scripts\Build-Native.ps1 -EngineRoot $engineRoot -Mode Package
~~~

The script calls `RunUAT.bat BuildCookRun` for Win64 Development with build, cook, stage, pak and archive enabled, and cooks `DepotBlock`. Archive output is under `Artifacts/Windows/`. Inspect UAT's actual archive path and locate the resulting executable:

~~~powershell
Get-ChildItem -LiteralPath .\Artifacts\Windows -Recurse -File -Filter '*.exe'
~~~

Run the archived game and repeat the ordinary-controls and mission checks after subsequent changes. Editor play and successful game-target compilation do not establish packaged compatibility. The final archive's content verification and the earlier Package04 ordinary-input results are recorded separately below.

## Launch the packaged build

The visual Package05 UAT run reported **BUILD SUCCESSFUL**, **exit code 0**, and **64.54 seconds** for BuildCookRun. Independently counted archive contents, excluding runtime `Saved/` files, are **48 files totaling 956,182,427 bytes**. These are development-build totals including symbols and supporting binaries. Actual executable paths relative to the checkout are:

- Bootstrap launcher: `Artifacts/Windows/Blackglass.exe`.
- Native game executable: `Artifacts/Windows/BLACKGLASS/Binaries/Win64/Blackglass.exe`.

Keep the entire archived directory together; the executable needs its cooked content and runtime files.

Package05 archive identities verified from the actual files:

| Archive-relative path | Bytes | SHA256 |
|---|---:|---|
| `BLACKGLASS/Binaries/Win64/Blackglass.exe` | 337,432,064 | `c7670656cc459ab5632a06dc775b86db1cbc15966b1f5633e1b4b3fbc82d78ca` |
| `BLACKGLASS/Content/Paks/BLACKGLASS-Windows.pak` | 10,668,474 | `3b676526434de64a5e7b226dc5d773791b48a2c10ff6f72f531c5868c47afc85` |
| `BLACKGLASS/Content/Paks/BLACKGLASS-Windows.ucas` | 123,781,696 | `45cf8ba2e23bc103469de0bbbffa730424c971442cb84c11087ef2a18b5a131d` |
| `BLACKGLASS/Content/Paks/BLACKGLASS-Windows.utoc` | 122,972 | `7c9f3598744f980b8854e6c25043b3b22f7c2bbf287dd9bf9c35e0b72770eabd` |

The installed UnrealPak tool listed the final IoStore map, `M_BlackglassSurface`, both original detail textures and their bulk data. Its pak listing confirmed `Content/Data/Weapons.csv`, `DefaultEngine.ini` and `DefaultInput.ini`. Targeted archived-config readback verified `DepotBlock` as the default map, `BGOperation` as the game mode, high-DPI game awareness enabled, and inherited debug bindings cleared for both player-input classes. This confirms what was cooked and archived; running appearance and control regressions require separate tests.

~~~powershell
powershell -ExecutionPolicy Bypass -File .\Scripts\Launch-Packaged.ps1 -Width 1920 -Height 1080
~~~

`Launch-Packaged.ps1` checks that both executables exist, starts the archive bootstrap from its own directory, and supplies `-windowed -ResX=1920 -ResY=1080 -ForceRes -NoSplash` by default. It runs the packaged game without launching Unreal Editor. The project enables high-DPI awareness; a real 1920x1080 game client was measured in the Package04 regression.

Ordinary physical F5/F9 input was tested in that build: saving/loading worked and retained normal lit rendering and the HUD. Local running-game captures `Saved/Verification/Game-20261002-221000.png` and `Game-20261002-221003.png` record the Audit-30 equipped state and the restored Ledger-12 state respectively. Engine debug shortcuts are cleared in this project's input configuration so F5 does not also switch into Shader Complexity and F9 does not also take a debug screenshot.

The Package04 ordinary-controls run acquired the specialist on foot, brought all four living operatives and the specialist to extraction, and resolved success with the 6,000-credit reward. A won state was saved and loaded without duplicating its reward; abort/restart and a four-operative van trip were also exercised. These are running-game results for that archive. Package05's darker district palette and HUD were reviewed in its actual 1920x1080 native game window. Fresh ordinary controls boarded all four operatives, drove a short west-street trip, stopped and disembarked all four. F9 restored the previously completed Package04 mission, and a Package05 F5/F9 round trip retained 6,000 credits, all four operatives and the specialist at the marker, with clean lit rendering. [District capture](Evidence/package05-district.png) and [restored-win capture](Evidence/package05-won-save-load.png) show the running package; the latter records a loaded prior win, rather than a new Package05 completion. The final bounded performance capture passed as recorded below; the package was not rebuilt for the profiling workaround. Read [VERIFICATION.md](VERIFICATION.md) for the latest recorded tests, package hashes and limitations.

## Profile the real packaged foundation

`Scripts/Profile-Foundation.py` launches the actual archived Win64 game at 1920x1080, collects two engine CSV captures, discards the first as warmup and analyzes the second. It uses the installed engine's supported `-csvNoProcessingThread` synchronous CSV-processing path together with `-ExitAfterCsvProfiling`. Earlier attempts reproduced a crash during CSV processing-thread shutdown; this tool flag avoided that failure without changing gameplay/render threading or rebuilding the package. It requires Windows, this project's package and a matching inspected engine installation. Close BLACKGLASS and Unreal Editor processes before an isolated capture; the runner refuses existing instances and does not close them.

Validate prerequisites without launching a game:

~~~powershell
python .\Scripts\Profile-Foundation.py --engine-root "$engineRoot" --run-label foundation-profile --frames 10000 --timeout 600 --min-duration 30 --validate-only
~~~

Then run the capture:

~~~powershell
python .\Scripts\Profile-Foundation.py --engine-root "$engineRoot" --run-label foundation-profile --frames 10000 --timeout 600 --min-duration 30
~~~

The default executable is `Artifacts/Windows/BLACKGLASS/Binaries/Win64/Blackglass.exe`; optional `--project-root` and `--executable` accept explicit paths, with validation that the latter is this project's actual packaged Win64 game. Keep the launched game window open. The runner terminates only its own process if the timeout expires.

A passing capture requires both the game and runner to exit zero, exactly two completed captures with both nominal frame counts confirmed by the engine, a measured physical 1920x1080 client, applied quality values, sufficient positive frame samples without invalid interior rows, and at least 30 seconds in the measured second capture. A clean early close or logical resolution setting alone does not pass these gates. Supported `--frames` is 2,000-1,000,000 and `--timeout` is 30-1,800 seconds; increase the frame count when a fast machine would finish the second capture in less than the required duration.

Local results are `Saved/Verification/<RunLabel>-<UTC timestamp>.summary.json` and `.context.json`, plus engine/stdout logs. The summary records hardware, driver, build, resolution, quality, actor counts, frame-time statistics and capture limitations. Raw CSV files remain under the packaged game's `BLACKGLASS/Saved/` directory. Keep raw logs and CSV metadata local; review any summary before publishing it.

The `foundation-final-certified` run passed with game exit 0 and runner exit 0. Both 10,000-frame nominal captures completed; the measured second capture retained **9,999 positive frame samples over 113.054326 seconds** at a physically measured **1920x1080**. Mean frame time was **11.306563 ms**, approximately **88.44 FPS**; median was **11.2302 ms**, 95th percentile **12.1444 ms**, and maximum **59.7132 ms**. The natural idle district contained 17 units, one van, two doors and 131 total actors on the tested integrated Intel Arc 140V system. Profiling overhead is included.

The measured average exceeds the 60 FPS target in this bounded idle scenario. It does not establish uninterrupted 60 FPS, complete-mission, heavy-combat or campaign stress performance. Missing or zero GPU timing means unavailable evidence; global thread times include waits. See [PERFORMANCE.json](PERFORMANCE.json) for the actual hardware, applied quality settings and profiling limits, and [VERIFICATION.md](VERIFICATION.md) for the recorded pass.


## Separate portable core fixture

The older engine-independent core remains a useful C++ contract fixture. It is not the native game and has no renderer, player input, animated actors or authored maps.

- State: `Source/BlackglassCore/include/blackglass/core.hpp` and `Source/BlackglassCore/core.cpp`.
- Definitions: `Data/Weapons.csv` (metres, hit points, seconds and carrying units).
- Contracts: `Tests/core_tests.cpp`.
- Build: root `CMakeLists.txt`.
- Windows test executable: `out/Debug/blackglass_core_tests.exe`.
- Linux test executable: `out/blackglass_core_tests`.

From the checkout root:

~~~powershell
cmake -S . -B out -DCMAKE_BUILD_TYPE=Debug
cmake --build out --config Debug --parallel 2
ctest --test-dir out -C Debug --output-on-failure --verbose
~~~

CMake requires a C++17 compiler. CTest runs one executable containing 24 integration contracts. Run its executable from the repository root so `Data/Weapons.csv` can be found. Actual tested compilers and results are recorded in [VERIFICATION.md](VERIFICATION.md).

The portable fixture's `BLACKGLASS_CORE` version 1 `.bgcore` saves are separate from native Unreal saves. The fixture uses 20 ms simulation ticks; pause freezes its mission clock. Frame partition tests cover movement and weapon cadence, without claiming general deterministic Unreal simulation.

## GitHub CI and Unreal runners

Hosted Windows core and MCP checks cover their own narrow scopes. The previously inspected GitHub-hosted Windows runner had no Unreal installation. Native builds require a legitimately installed matching engine and Windows C++ toolchain on an owner-controlled machine or runner.

The existing environment workflow can select an owner-controlled runner through these repository Actions variables:

| Variable | Value |
|---|---|
| `BLACKGLASS_WINDOWS_RUNNER` | JSON array of actual runner labels, for example `["self-hosted","Windows","X64","blackglass-unreal"]` |
| `BLACKGLASS_UE_ROOT` | The engine root on that runner |

These are examples; registering a runner and setting actual labels are separate steps. Without a configured runner, the probe falls back to `windows-2022` and fails explicitly when Unreal is absent. A passing environment probe is prerequisite evidence, not gameplay evidence.
