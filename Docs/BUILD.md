# Build and verification instructions

There is no playable Unreal game yet. The available build is an engine-independent C++ simulation library plus a console test executable. It has no renderer, window, player input, animated actors or authored maps. Do not treat its test output as gameplay evidence.

## Current source and output paths
- C++ state: Source/BlackglassCore/include/blackglass/core.hpp and Source/BlackglassCore/core.cpp.
- Editable definitions: Data/Weapons.csv (metres, hit points, seconds and carrying units).
- Integration contracts: Tests/core_tests.cpp.
- Configure/build files: CMakeLists.txt.
- Windows test output after compilation: out/Debug/blackglass_core_tests.exe.
- Linux test output: out/blackglass_core_tests.
- No .uproject, playable .umap or packaged game executable exists.
- No player controls or game launch instructions exist yet.

## Run the core contracts
From the checked-out repository root on the development branch:

~~~shell
git clone --branch blackglass/milestone-1 https://github.com/Velamj/BLACKGLASS.git
cd BLACKGLASS
cmake -S . -B out -DCMAKE_BUILD_TYPE=Debug
cmake --build out --config Debug --parallel 2
ctest --test-dir out -C Debug --output-on-failure --verbose
~~~

CMake requires a working C++17 compiler. The actual tested compilers are recorded in VERIFICATION.md. CTest runs one executable containing 24 integration contracts. Directly running the executable also requires the repository root as working directory, so Data/Weapons.csv is found.

Core save format: BLACKGLASS_CORE version 1, with .bgcore used in tests. This is not an Unreal campaign save format. Writes use a same-directory temporary file, replace the primary safely, retain a .previous copy when the existing primary parses correctly, and reject incompatible or inconsistent data without mutating the live world. Windows replacement uses MoveFileExW; Linux uses filesystem rename. Simultaneous writes to one slot are unsupported.

Simulation ticks are 20 ms. Pause freezes the mission clock. No background campaign progression or wall-clock progression occurs. Advance accepts elapsed intervals up to 60 seconds; callers should provide frame elapsed time. Frame partition tests cover movement and weapon cadence; no general engine determinism is claimed.

## Inspect an actual Unreal development installation
The toolchain probe does not install, upgrade or download Unreal. It inspects the configured root, Epic's standard installation folder and registered engine paths.

~~~powershell
$env:BLACKGLASS_UE_ROOT = "C:\actual\installed\engine"
pwsh -File Scripts/Inspect-Environment.ps1 -RequireUnreal
~~~

The root must contain Engine/Build/Build.version, Engine/Build/BatchFiles/Build.bat, Engine/Build/BatchFiles/RunUAT.bat and an actual Windows editor executable. The script reads Build.version before an engine version or version-specific integration is selected. Reports are written to Artifacts/Toolchain/environment.json.

## Local Windows editor bridge

The owner selected their Windows PC, which does not have Unreal installed yet. [UNREAL_MCP.md](UNREAL_MCP.md) gives official installation links, the pinned ue-mcp 1.3.9 setup, local Codex configuration and exact connection checks. A verified MCP server process is separate from a compiled Unreal bridge and connected editor.

## GitHub Unreal prerequisite
Current GitHub-hosted Windows probes found zero Unreal installations. Register an owner-controlled Windows Actions runner that already has legitimately installed Unreal, its matching C++ toolchain and Windows SDK. Then configure these repository Actions variables:

| Variable | Value |
|---|---|
| BLACKGLASS_WINDOWS_RUNNER | JSON array of the actual runner labels, for example ["self-hosted","Windows","X64","blackglass-unreal"] |
| BLACKGLASS_UE_ROOT | Actual absolute engine root on that runner |

The example label must be replaced or registered to match the actual runner. With no runner variable, the probe falls back to windows-2022 and fails explicitly if Unreal is unavailable. Run the Windows and Unreal toolchain workflow on this branch again and retain its actual report.

A passing environment probe will establish dependencies, not game playability. The next implementation pass must create/compile the actual Unreal project, use that engine's navigation/input/UI/animation APIs, build the urban block and validate ordinary controls, camera rendering, four animated operatives, weapons, civilians/security, a vehicle, objective/extraction/failure/restart and save/load before calling Milestone 1 complete.

No engine-specific API, plugin capability, successful editor connection, playable map, screenshot, 60 FPS result or packaged compatibility is assumed.
