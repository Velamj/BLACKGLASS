# Verification evidence — updated 2026-10-02

**Milestone 1 is incomplete. No game playtest has run.**

## Tested source
C++ source commit: [9b82395b1bf30a94fb2f839aea51d9bd3363f294](https://github.com/Velamj/BLACKGLASS/commit/9b82395b1bf30a94fb2f839aea51d9bd3363f294).

[Core C++ workflow run 36910506263](https://github.com/Velamj/BLACKGLASS/actions/runs/36910506263):
- Windows job 110531675507: compiled with MSVC 19.44.35229.0; **24/24 core contracts passed**.
- Ubuntu job 110531675893: compiled with GNU 13.3.0; **24/24 core contracts passed**.
- Both builds enforce warnings as errors.
- CTest reports one test executable on each platform; each executable runs the 24 named integration contracts in Tests/core_tests.cpp.
- Actual JUnit artifacts were uploaded: blackglass-core-windows-2022-36910506263 (artifact 11186481707), blackglass-core-ubuntu-24.04-36910506263 (artifact 11186236897).
- Artifact retention is 14 days; this document retains run/commit links after artifacts expire.

Coverage includes four-operative selection/group movement, individual queues/stop, door route changes, narrow-route/traffic yielding, obstruction/corner rays, actual friendly-fire interception, armor/two weapon roles, locally observed/heard security incidents, civilian flight, pause/frame partitioning, weapon cadence, transactional inventory transfer, seats/driver orders, blocked exits/wreck occupancy, vehicle shot obstruction, acquisition/extraction/one-time rewards, survivor requirements, target/roster failure/abort, controller loss, active-mission resume, corrupt-save rejection and safe-write backups.

An initial formation contract caught displaced units stopping short of their destination waypoint. The corrected implementation and same-cell regression contract pass on both actual compilers. No contract was removed or weakened to pass.

These are standalone simulation contracts. They do not prove the game accepts mouse/keyboard orders, the Unreal navigation system works, animation/effects correspond to hits, the camera is orthographic, or a Windows game packages and runs.

## Actual Windows/Unreal environment
[Environment workflow run 36910506132](https://github.com/Velamj/BLACKGLASS/actions/runs/36910506132), job 110531675700, same tested source commit:
- OS: Microsoft Windows Server 2022 Datacenter, 10.0.20348.
- Runner image: win22, 20260927.320.1.
- CPU: AMD EPYC 9V74, 2 assigned cores / 4 logical processors.
- Memory: 15.99 GiB.
- Graphics adapter: Microsoft Hyper-V Video, driver 10.0.20348.1.
- Visual Studio Enterprise 2022, installation 17.14.37710.0.
- CMake: 3.31.6.
- Windows SDK includes present; 10.0.26100.0 is among detected versions.
- Discovered complete Unreal installations: **0**.
- Check conclusion: **failure**, because Unreal is missing. The report upload succeeded.

Exact missing dependency: an accessible Unreal Engine installation containing Engine/Build/Build.version, Engine/Build/BatchFiles/Build.bat, Engine/Build/BatchFiles/RunUAT.bat and a Windows editor executable. No actual engine version, editor integration or engine API has been verified.

The probe searched the configured root (if any), Epic's standard program directory and registered Unreal builds; an undisclosed installation at another path is not ruled out. A supplied installation must be probed before use.

This VM is a compiler/test machine, not the documented midrange graphics test system required by the brief. No resolution, quality setting, actor-count performance result, GPU frame time, CPU profiler trace or 60 FPS claim exists.

## Published Unreal MCP tooling

Tooling source commit: [a19b4317748d07aee061bd79dacfaa1a220bc209](https://github.com/Velamj/BLACKGLASS/commit/a19b4317748d07aee061bd79dacfaa1a220bc209).

[Windows MCP workflow run 36922455754](https://github.com/Velamj/BLACKGLASS/actions/runs/36922455754), job 110571567780, passed on 2026-10-01:
- Node.js 22.23.3, win32; published ue-mcp package 1.3.9.
- Real stdin/stdout MCP initialize response: server ue-mcp 1.3.9, protocol 2024-11-05.
- Real tools/list response: 27 advertised tools, including project.
- Real project(get_status) response: mode disconnected, editorConnected false, project null, editorTarget.projectPath null, legacy port 9877.
- Four checks passed. The separate pull request run 36922462870 also passed.
- Artifact unreal-mcp-windows-probe (11192232303) retained the actual report and generated package-lock.json for 14 days.

This proves that the pinned published Node server installs and speaks MCP on Windows. It does not prove the C++ bridge compiles, that any advertised editor operation works, or that Unreal is connected. No upstream demo or flow was executed.

The owner selected their Windows PC and confirmed Unreal is not installed there yet. Engine installation and a connected PC session are required; the repository does not install software onto that PC.

## Additional local core check

The public source at commit 27b8cfbd0ce62f81cd71bfe5c6e19903d382b057 was checked out and compiled locally using the existing C++17 requirements with warnings as errors. All **24/24** portable core contracts passed, compilation/test exits were 0, and the working tree remained clean. There were no game-source edits.

Detailed machine, filesystem and local diagnostic reports are retained locally rather than uploaded to this public repository. This additional check verifies existing state only; it does not establish Unreal gameplay or Windows game packaging.

The owner started installing Unreal and reports a storage blocker. Exact install requirements remain pending, and the remote connection is currently unresponsive. No installer alteration, cleanup or personal-file removal has been performed.

## Game acceptance status
All twelve user acceptance tests remain pending in the running game. Partial core coverage must not be marked as in-game acceptance.

| Acceptance test | Current evidence/gap |
|---|---|
| 1. Four individual/group orders | Core contracts pass; ordinary controls/animated operatives absent |
| 2. Doors, traffic, narrow/vertical routes | Core horizontal fixture contracts pass; actual engine navigation/accessible floors absent |
| 3. Obstruction-aware shooting | Core ray/vehicle/interceptor contracts pass; actual engine collision/effects absent |
| 4. Civilian/security event response | Core local-evidence/flight contracts pass; functioning city, perception presentation and wider escalation absent |
| 5. Functional neural controls | Not implemented/tested |
| 6. Mission completion using neural override | Not implemented/tested; escort link is not neural override |
| 7. Boarding/drive/destruction/exit consistency | Core contracts pass; physical vehicles and controls absent |
| 8. Persistent equipment/casualties/rewards/research | Core equipment/death/reward state covered; research/campaign absent |
| 9. Representative active-mission save/load | Core actor/seat/follower/order/clock state roundtrip passes; Unreal runtime restoration absent |
| 10. Alternate mission approaches | No authored playable mission or alternate-approach playtest |
| 11. Campaign changes subsequent play | Campaign and subsequent operation not implemented |
| 12. Performance and packaging evidence | Dependency inspection only; engine/game/performance/packaging absent |

## Paths, launch and screenshots
Source/data/test paths and reproducible commands are in BUILD.md. No .uproject, .umap or packaged game exists. No game launch or player control mapping exists. No running-game screenshots have been captured; no generated images or source inspection are substituted.

## Next action
Install Unreal on the owner's Windows PC and connect its terminal/editor session as documented in UNREAL_MCP.md, then run the environment probe. An Unreal-equipped self-hosted Windows Actions runner remains an alternative documented in BUILD.md. With that verified installation, create the engine project and implement/test the actual Milestone 1 urban operation.
