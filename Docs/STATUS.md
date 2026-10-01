# Development status

PROJECT BLACKGLASS remains an original real-time isometric offline Windows PC game project. The working title is not a cleared commercial brand.

**Milestone 1 is not complete. There is no playable game, Unreal project file, map or packaged executable yet.** Engine integration remains blocked on verified access to the owner's Windows development PC.

## Completed repository work
- Preserved the initial main commit/README and developed on blackglass/milestone-1.
- Added original portable C++ state for four-operative orders/movement, collision-aware weapons, eight inventory slots/weight/transfer, local civilian/security reactions, vehicle driver/seats/damage/exits, specialist escort/objective/outcomes and versioned persistent state.
- Added editable weapon CSV, reproducible CMake builds and Windows/Linux CI.
- All 24 core integration contracts pass on actual MSVC and GNU builds. See VERIFICATION.md.
- Preserved all seven instruction parts in MASTER_BRIEF.md, including the later campaign/vertical-slice requirements.
- Recorded incomplete systems in FIDELITY_LEDGER.md and the absence of art/audio/maps in ASSET_MANIFEST.md.

## Blocker
The actual GitHub Windows runner has C++ tools and SDKs but zero discovered Unreal installations. Engine-specific integration, camera/rendering, gameplay input, authored map, animations, physical gameplay, screenshots and packaged-game testing cannot run there. Engine version has not been assumed.

The owner selected their Windows PC and ue-mcp v1.3.9. The installed engine path and an operational PC/editor connection are still pending. The pinned Node tooling and a Windows MCP process probe are now provided; see UNREAL_MCP.md. A self-hosted Windows runner remains an alternative, configured using BLACKGLASS_WINDOWS_RUNNER and BLACKGLASS_UE_ROOT as described in BUILD.md. Inspect the exact version/toolchain before creating engine-specific files.

## Known limitations
- This is a standalone state prototype; it is not an Unreal module or playable substitute.
- Navigation/collision are 2D fixture logic with cell-centre paths; no engine navigation, floors, stair/bridge traversal, crowd solver or full vehicle physics exists.
- The two weapon definitions have different functional values but no final animation, handling, accuracy, projectile behavior, audio/effects, concealment tuning or playtest balance.
- Security has local sight/hearing and remembered crime state; complete suspicion, communications, escalation and patrol/routine city simulation remain absent.
- Specialist acquisition is a plain escort fixture. Neural settings and neural override are not implemented.
- Actor shooting into/from occupied vehicles and vehicle attack orders return an explicit unavailable reason. Damage state is tested through core APIs.
- Core inventory has slots, weights, equipping/switching/transfer. Campaign purchase/resupply/sale, world drops/pickups and recovered research tech remain absent.
- Persistent data covers implemented state only. Campaign economy/research/territories, cybernetics, neural-control progression, reserve roster and recovery/recruitment/ironman are not implemented.
- No mouse/keyboard mapping, UI, camera, environment art, character rig/animation, sound or accessibility presentation exists. There are no player-facing placeholders labeled complete.
- No ordinary-controls playtest, gameplay screenshot, profiler or packaged-game test has run. The GitHub VM does not establish the midrange 60 FPS target.
- The cell simulation is intended for small integration fixtures, not a city-scale performance result. Further optimization requires an actual engine/profile.

## Next bounded implementation step
Verify the supplied engine, create the native Unreal C++ project, integrate authoritative state with engine-supported navigation/input/UI/animation, and build one authored isometric urban block with four animated selectable operatives, two weapons, civilians/security, one vehicle and a complete objective/extraction/failure/restart/save-load loop. Test it through ordinary controls and capture real screenshots before closing Milestone 1.

Milestones 2–4, Hostile Acquisition (15–25 minutes), at least ten substantive alpha operations and the eventual 50-territory authored campaign remain in scope. Zero authored operations or territories have been counted complete.
