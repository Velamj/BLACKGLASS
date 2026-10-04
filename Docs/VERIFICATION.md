# Verification evidence — updated 2026-10-03

## Current native runtime result

**The native foundation passed one actual runtime integration scenario. The revised Windows package built, launched and passed ordinary group vehicle controls and save/load checks; an earlier package completed the mission through ordinary controls. The final package also completed a measured 1080p foundation-scene run with clean shutdown using a supported CSV-profiler workaround.**

Unreal Engine **5.8.3** was verified from the installed engine. The native editor target, gameplay module and vendored ue-mcp C++ bridge compiled successfully on Windows. The real `/Game/Maps/DepotBlock` map loaded and generated Recast navigation; this was not a portable fixture or source-only inspection.

The expanded `Blackglass.Foundation.Runtime` scenario passed in **116.17 seconds** on 2026-10-03. The engine Automation report recorded **1 successful scenario with warnings, 0 failed, 0 not run and 0 in process**, with no error events. The native process and report-gated Python runner both exited 0. A zero process exit alone is insufficient: earlier failed runs also exited 0, and the runner correctly rejected their failed reports.

The report retains one initialization warning: `LogCrowdFollowing: Unable to find RecastNavMesh instance while trying to create UCrowdManager instance`. Actual navigation, mission completion, restart and the subsequent vehicle approach checks still passed. This warning remains documented.

The one scenario exercised these native systems through actual world ticks:

- Individual and group selection, one-recipient movement, four operative movement destinations and queued street orders.
- Solid-wall shot rejection, damaged-door interception, ammunition use and target damage after opening the obstruction.
- Local hearing and witnessed-crime memory without instant distant awareness; holstering retained the witnessed incident.
- Queued attack intent during genuine tactical pause: health, ammunition and mission time stayed unchanged until resume.
- Actual movement to board the vehicle, collision-aware vehicle travel, driver/seat relationships, moving-route and alarm restoration, blocked exits and occupied-vehicle destruction. After restart, all four default operatives walked to board through the ordinary group control, owned distinct matching seats and disembarked consistently.
- A height-sensitive boarding boundary inside planar range but outside spatial range retained its approach order. Horizontal navigation was observed before boarding, followed by matching attachment/seat state and clean disembarking.
- Native disk save/load of entity identity, selection, health, ammunition, pending reload, casualty equipment, queued movement, vehicle attachment, barriers and active security state. Duplicate identities and a corrupted primary save were rejected without changing live state; a previous valid save remained available.
- An ordinary interaction opened the facility gate, and the following single movement order survived navigation regeneration and crossed it. An independent operative climbed the accessible raised route.
- Specialist acquisition and escort through actual orders. Partial extraction remained active with no revenue; all surviving operatives then walked to extraction, producing one 6,000-credit reward. Loading and reevaluating success did not duplicate it.
- Invalid successful saves with an outside survivor or no survivors were rejected. Mission-critical target death, total squad loss and ordinary restart produced explicit, consistent outcomes.

Civilians and guards were held still for isolated contracts while their collision, damage and awareness callbacks remained real. Fixture placement established shooting and occupancy conditions; it was not used as movement evidence. This scenario does not establish natural crowd/patrol balance, a complete physical-input playthrough, visual quality, performance or packaged compatibility.

Runs 01–04 exposed a witness fixture alignment issue, vehicle roof navigation, a physically blocked vehicle lane and gate navigation regeneration. Run 05 passed the earlier scenario. Expanded Run 06 passed the mission and default group boarding but failed its isolated boundary fixture because an exited teammate physically blocked the boarding trace. Run 07 retained collision and commanded that teammate to actually walk clear before establishing the boundary condition; the full scenario then passed. No failed assertion was discarded.

## Reproduce and inspect locally

Build the native editor target first using [BUILD.md](BUILD.md). From the project root:

```powershell
python Scripts/Verify-Foundation.py --engine-root "C:\Program Files\Epic Games\UE_5.8" --run-label foundation --timeout 600
```

The test is in `Source/Blackglass/Private/BlackglassTests.cpp`. The runner validates the actual engine report, imposes a process limit and preserves pre-existing Autosave files. Run it without another game instance writing saves.

The passing run's local evidence is:

- `Saved/Automation/foundation-runtime-07-20261003T035237Z/index.json`
- `Saved/Verification/foundation-runtime-07-20261003T035237Z.summary.json`
- Corresponding `.engine.log` and `.stdout.log` files under `Saved/Verification`.

Raw reports and captures remain in ignored local output; private machine identifiers, filesystem listings and diagnostic metadata are not published. The recorded result applies to the locally compiled source used for that run; it is not a claim that GitHub CI built Unreal.

## Revised Windows package result

Final archive 05 completed actual Windows Development packaging with UAT exit 0 in **64.54 seconds**, cooking the original textures, material, map, data and configuration. Its native game launched at a physically verified **1920×1080**. The revised palette, material surfaces, district dressing, operative coats and interface were inspected in the running game.

From fresh deployment, physical `Space` and `E` selected the squad and boarded all four operatives without moving one closer manually. Minimap interaction and a right-click commanded a short western-street vehicle journey; `X` stopped it and group `E` disembarked all four. `F8` restart produced a fresh operation.

`F9` loaded the genuinely completed archive 04 save into archive 05. Physical `F5`/`F9` then saved and loaded that completed state again, retaining **6,000 credits**, all four operatives and the specialist at extraction. This proves compatibility and the final package's won-state roundtrip; it is not a new archive 05 mission-completion playthrough.

Actual running-game evidence: [revised district](Evidence/package05-district.png) and [loaded completed state after final-package save/load](Evidence/package05-won-save-load.png). These are game captures, not generated previews. The local package launcher is `Artifacts/Windows/Blackglass.exe`; the actual native binary is `Artifacts/Windows/BLACKGLASS/Binaries/Win64/Blackglass.exe`.

Foreground occlusion and agent outlines still require implementation. The separately measured performance result below applies to its bounded idle scene.

## Packaged performance result

Final archive 05 completed the formal capture on 2026-10-03 with **actual game exit 0 and runner exit 0**. Both captures contained 10,000 nominal frames and complete final CSV headers/footers. The first capture was discarded as warmup. The second contained **9,999 positive frame-time samples over 113.054326 seconds**, one boundary row and no interior timing gaps. Its physical game client was measured at **1920×1080** using a scoped per-monitor V2 DPI context.

The measured frame mean was **11.306563 ms**, approximately **88.44 FPS** in this scene. The timing distribution was:

| Measurement | Mean ms | p50 ms | p95 ms | Maximum ms |
|---|---:|---:|---:|---:|
| Frame | 11.306563 | 11.2302 | 12.1444 | 59.7132 |
| Game thread | 2.405621 | 2.3314 | 2.88294 | 41.3316 |
| Render thread | 11.300851 | 11.2269 | 12.09591 | 59.6883 |
| GPU | 9.836859 | 9.7523 | 10.19912 | 15.4103 |

This was an unpaused DepotBlock with natural NPC simulation and no player orders. Runtime queries confirmed **17 units, one vehicle and two doors**; CSV samples retained 17 units and 131 total actors throughout. Actor counts include world helpers and hidden actors, rather than describing on-screen objects. Global thread times include waits; these four measurements do not identify a specific bottleneck. Combat and full-mission performance still require measurement, and the maximum frame time demonstrates occasional stalls.

The tested hardware was an Intel Core Ultra 7 258V, 8 cores/8 logical processors, 31.54 GiB RAM and an **integrated Intel Arc 140V** on Windows 11 Home. Its adapter-reported “16GB” label describes shared memory rather than 16GB of dedicated VRAM. GPU driver was 32.0.101.8132. Applied queries recorded screen percentage 100, shadow quality 2, antialiasing method 2, VSync 0 and an uncapped frame rate. View distance, effects, textures and postprocessing were 3; dynamic global illumination, reflections and virtual shadows were 0.

Sanitized aggregates are in [PERFORMANCE.json](PERFORMANCE.json). The passing local summary is `Saved/Verification/foundation-final-certified-20261003T043222Z.summary.json`, with corresponding engine/stdout logs and ignored packaged CSVs. Both CSVs were independently reread and compared with the summary before publishing the aggregates. Private footer metadata, device identifiers and raw diagnostics remain local.

## CSV-profiler shutdown history

The initial complete 10,000-frame-pair run returned **777003** and its runner returned 1. The installed engine defines that status as `CrashReporterCrashed`, so the log's requested exit 0 was insufficient. A D3D11 diagnostic also returned 777003; the evidence did not justify a renderer-specific fallback. The failed summary remains at `Saved/Verification/foundation-final-visual-20261003T035928Z.summary.json`. Its original client-size observation was lost by the earlier runner and is not used as resolution proof.

A locally captured first-chance access-violation dump resolved with the matching game PDB to allocator `FPerThreadFreeBlockLists::ClearTLS` called from Unreal's `FCsvProfilerProcessingThread::Run`. The stack contained no project camera/map cleanup frames. Source verification found the engine-supported **`-csvNoProcessingThread`** flag, which avoids creating that CSV worker. The passing formal rerun used this flag with supported automatic exit after CSV completion on the same archive 05.

CSV processing runs synchronously for this measurement; its overhead is included. Gameplay and rendering remain threaded. This is a profiling workaround, with no game-source, renderer-configuration or package change. An intermediate window-close harness failed before requesting close and was removed; a short probe closed early and was not accepted as complete-capture evidence. The formal run still required zero exit, physical resolution, two complete captures, duration, quality and actual actor counts.

Reproduce the formal capture with the game/editor closed and leave its window open until it exits automatically:

```powershell
python Scripts/Profile-Foundation.py --engine-root "C:\Program Files\Epic Games\UE_5.8" --run-label foundation-profile --frames 10000 --timeout 600
```

## Earlier ordinary-control mission playthrough

The actual Windows Development package from archive 04 completed the mission through physical controls on 2026-10-03. From a fresh restart, `Space` selected all four operatives; minimap interaction and right-click movement took them to the main gate. A real right-click opened the gate; further movement reached the facility; a right-click on the specialist established the escort at 03:22:32 UTC. Selecting the squad and right-clicking extraction took all four operatives and the specialist there, producing success and exactly 6,000 credits at 03:23:39 UTC.

Physical `F5`/`F9` saved and loaded the won state while preserving 6,000 credits. Separate `F8` restart and `F7` abort checks produced the displayed outcomes and a fresh level. Earlier ordinary checks also demonstrated individual movement, tactical pause, weapon switching and active-mission quicksave/load restoring selection and mission time.

Ordinary `E` boarding, a short western-street vehicle journey, `X` stop and group `E` disembarking also passed, after moving the fourth operative closer to the vehicle. Initial group boarding exposed a real boundary defect: the order checked planar range while boarding checked spatial range. Source now uses the same spatial range. The compiled, expanded native scenario subsequently passed all four default operatives walking and boarding through the group control, plus a real approach at that boundary. Ordinary group boarding, travel, stop and exit also passed independently in archive 05, as recorded above.

Actual running-game captures are retained locally as `Saved/Verification/Game-20261002-222407.png` (success), `Game-20261002-222426.png` (loaded success) and `Game-20261002-222455.png` (abort). Filenames use the capture machine's local date; the playthrough above is dated in UTC. Raw captures have not been published by this document.

The mission-completion playthrough above is an archive 04 result, before the visual revision. Archive 05's build, visual inspection, vehicle controls and loaded-state checks have their separate evidence above. The final archive's bounded performance measurement and the CSV-worker workaround have separate evidence above.

## Brief acceptance status

These are partial Milestone 1 results, not completion of the twelve full-game acceptance criteria.

| Acceptance test | Current native evidence and remaining gap |
|---|---|
| 1. Four individual/group orders | Native individual/group/queued orders pass; ordinary packaged individual movement and the full four-operative escort/extraction playthrough passed in archive 04. |
| 2. Doors, traffic, narrow/vertical routes | Opened-gate crossing, actual vehicle route collision and raised-ramp navigation pass. Wider crowd/traffic yielding and service-route playtests remain pending. |
| 3. Obstruction-aware shooting | Actual native wall rejection, door interception, ammunition and target damage pass. Rendered hit effects and full weapon handling require live review. |
| 4. Civilian/security event response | Controlled native local hearing, witnessed crime, distant ignorance and remembered evidence pass. Natural civilian panic, patrol balance and full escalation remain incomplete. |
| 5. Functional neural controls | Not implemented or tested. |
| 6. Mission completion using neural override | Not implemented or tested. The tested specialist escort is not neural override. |
| 7. Boarding/drive/destruction/exit consistency | Native distant and default four-operative group boarding, the height-sensitive real approach, swept travel, occupancy restoration, blocked exits and occupied wreck states pass. Archive 05 ordinary fresh group boarding, travel, stop and group exit pass without a manual closer movement. |
| 8. Persistent equipment/casualties/rewards/research | Native ammunition, casualty equipment and one-time revenue persist. Equipment recovery, research and the campaign loop remain absent. |
| 9. Representative active-mission save/load | Native entities, active reload/queued movement, moving vehicle/occupants, barriers and alarm state restore without duplicate identities. Corrupt data preserves live state. Neural/campaign persistence remains absent. |
| 10. Alternate mission approaches | One holstered escort approach completed through ordinary packaged controls, as well as the native scenario. Two full authored approaches, neural manipulation and the polished vertical slice remain pending. |
| 11. Campaign changes subsequent play | Campaign progression and a subsequent operation are not implemented. |
| 12. Performance and packaging evidence | Archive 05 packaging, actual 1080p launch, visual inspection, vehicle controls and save/load are proven. Its bounded idle capture passed clean shutdown with the supported CSV-worker workaround: 11.306563 ms mean and 12.1444 ms p95. Archive 04 ordinary mission completion is proven. Combat/full-mission performance remains unmeasured. |

## Historical portable-core evidence

These results cover the standalone C++ fixture, not Unreal gameplay. They remain useful regression history but do not add to the native test count.

- Source [9b82395b1bf30a94fb2f839aea51d9bd3363f294](https://github.com/Velamj/BLACKGLASS/commit/9b82395b1bf30a94fb2f839aea51d9bd3363f294): [Core C++ workflow 36910506263](https://github.com/Velamj/BLACKGLASS/actions/runs/36910506263) passed **24/24 named contracts** under actual Windows MSVC and Ubuntu GNU builds, with warnings as errors. CTest reported one executable on each platform; each executable ran all 24 contracts in `Tests/core_tests.cpp`. JUnit artifacts were retained for 14 days.
- Source [27b8cfbd0ce62f81cd71bfe5c6e19903d382b057](https://github.com/Velamj/BLACKGLASS/commit/27b8cfbd0ce62f81cd71bfe5c6e19903d382b057): an additional local C++17 compile/test passed all 24 contracts with exit 0 and a clean working tree.
- Source [3bb28474db18890173038c08e25797231b5209e1](https://github.com/Velamj/BLACKGLASS/commit/3bb28474db18890173038c08e25797231b5209e1): the separate Windows checkout configured, built and passed all 24 contracts with exit 0. Its then-verified Unreal 5.7.4 installation has been superseded by the verified 5.8.3 installation used above.

The portable contracts cover their fixture's orders, queues, obstruction, local incident response, pause/timing, inventory, seats/wrecks, objectives and persistent state. They do not prove Unreal navigation, animation, mouse/keyboard controls or packaging.

## Historical prerequisite and MCP probes

[Environment workflow 36910506132](https://github.com/Velamj/BLACKGLASS/actions/runs/36910506132) detected no complete Unreal installation on its hosted Windows runner and failed its prerequisite check. This historical CI limitation does not describe the current local Windows engine installation. The hosted machine did not establish the requested midrange graphics performance target.

Tooling source [a19b4317748d07aee061bd79dacfaa1a220bc209](https://github.com/Velamj/BLACKGLASS/commit/a19b4317748d07aee061bd79dacfaa1a220bc209): [Windows MCP workflow 36922455754](https://github.com/Velamj/BLACKGLASS/actions/runs/36922455754) passed four checks with Node.js 22.23.3 and ue-mcp 1.3.9. Actual stdio initialization returned protocol 2024-11-05; tool discovery advertised 27 tools; project status explicitly reported disconnected, with no editor/project. The separate [pull request run 36922462870](https://github.com/Velamj/BLACKGLASS/actions/runs/36922462870) also passed. The probe artifact was retained for 14 days.

That historical probe proves the pinned Node server speaks MCP. The current native bridge also compiles, but a live editor connection or successful editor command requires its own evidence in [UNREAL_MCP.md](UNREAL_MCP.md).

## Next bounded checks

Implement foreground occlusion and agent outlines, then continue the bounded Milestone 2 work for functional neural controls and alternate mission approaches. Retain the separate native, ordinary-control and rendered-game evidence; each supports its specific claims. Measure combat and full-mission performance when those systems and content are ready. Keep remaining fidelity gaps explicit in [FIDELITY_LEDGER.md](FIDELITY_LEDGER.md). Milestones 2–4 and all seven parts of [MASTER_BRIEF.md](MASTER_BRIEF.md) remain in scope.