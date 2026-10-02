# BLACKGLASS

Original real-time isometric corporate tactics game for offline Windows PC, with the 1993 PC Syndicate as a mechanics benchmark. All game content must be original or properly licensed.

**Milestone 1 is incomplete. This repository currently contains a tested C++ simulation core; there is no playable Unreal game yet.** The actual GitHub Windows runner has no Unreal installation.

Development source: [blackglass/milestone-1](https://github.com/Velamj/BLACKGLASS/tree/blackglass/milestone-1).

## Verified development work
The original core handles four-operative selection/orders/movement, two editable weapon definitions with obstruction and friendly fire, inventory weight/transfer, local civilian/security responses, vehicle seats/driver/damage/exits, specialist escort/extraction/failure, and versioned save/load with prior valid backups.

[All 24 core contracts pass on Windows and Linux](https://github.com/Velamj/BLACKGLASS/actions/runs/36910506263). They are simulation tests, not in-game playtests. [The Unreal toolchain check records the exact missing dependency](https://github.com/Velamj/BLACKGLASS/actions/runs/36910506132).

## Build the core tests
From this branch's repository root:

~~~shell
cmake -S . -B out -DCMAKE_BUILD_TYPE=Debug
cmake --build out --config Debug --parallel 2
ctest --test-dir out -C Debug --output-on-failure --verbose
~~~

There is no game launch command or player control mapping yet. [Build instructions](Docs/BUILD.md) give current file/output paths and the Windows runner/engine configuration required to implement the actual game.

## Unreal editor setup
Windows remains the game target. [Pinned ue-mcp 1.3.9 setup](Docs/UNREAL_MCP.md) documents prerequisites, local Codex configuration and connection verification. The owner has started the Unreal installation. [The real Windows MCP server check passed](https://github.com/Velamj/BLACKGLASS/actions/runs/36922455754); successful engine installation and editor/toolchain verification remain pending.

## Scope and evidence
- [Full seven-part master brief](Docs/MASTER_BRIEF.md).
- [Development status and known gaps](Docs/STATUS.md).
- [Fidelity ledger: retained mechanics, new additions and evidence](Docs/FIDELITY_LEDGER.md).
- [Verification results and acceptance-test status](Docs/VERIFICATION.md).
- [Asset manifest](Docs/ASSET_MANIFEST.md).

The first playable foundation, Hostile Acquisition vertical slice, campaign alpha and eventual 50-territory campaign remain the objective. No absent map, fixture or interface entry is counted as completed content. PROJECT BLACKGLASS is a working title, not a cleared commercial brand.
