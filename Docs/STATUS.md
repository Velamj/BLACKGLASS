# Development status

BLACKGLASS is a native, offline Windows real-time isometric tactics project. Its working title is not a cleared commercial brand.

**Milestone 1's playable foundation is tested in Unreal Engine 5.8.3 and a local Development Windows package. A bounded visual pass is implemented and inspected in the running executable. Art remains procedural placeholder work; the polished vertical slice and full campaign are not complete.**

## Playable foundation

- Depot Block contains streets, a controlled facility, two interactive/destructible gates, a narrow service route and a raised bridge with accessible ramps.
- Four independently selectable operatives support formation destinations, queued orders, stop/hold, holstering, two weapon choices and reloads. Articulated primitive figures walk, aim and recoil.
- A true orthographic camera supports pan, edge scrolling, zoom, stepped rotation, recentering and tracking. Four status panels, objectives, alerts and an interactive detection-filtered minimap show actual state.
- CSV-defined sidearm and automatic weapons use real collision, ammunition, cadence, spread, range and damage. Doors and the van receive damage; structural destruction is absent.
- Eight civilians walk simple routes and react locally to danger. Four initial guards patrol, investigate sight/sound events and remember crimes; bounded reinforcements can respond.
- A six-seat van supports approach/boarding, driver orders, swept street movement, collision stopping, disembarking, damage, destruction and saved occupants.
- Interact with Iona Vale, then escort her and every surviving operative into extraction. Success pays 6000 credits once. Target loss, total squad loss, abort and restart are explicit.
- Optional tactical pause permits orders while freezing simulation. F5/F9 quicksave/load and autosaves preserve implemented state using stable IDs, validation, checksum, staged writes and a previous valid save.
- The visual pass adds original seeded concrete/asphalt textures, a material graph, 1062 noncolliding dressing instances, clearer coat/face silhouettes, outlined labels, notice wrapping and corrected minimap roads. The established collision, navigation and camera structure is retained.

## Verified engineering and play

Unreal 5.8.3, MSVC 14.50.35728 and Windows SDK 10.0.26100.0 compiled the editor/game modules and the editor-only ue-mcp 1.3.9 bridge. The real map is Content/Maps/DepotBlock.umap. Native actor state owns gameplay; the portable simulation is a separate test fixture.

Blackglass.Foundation.Runtime passed in 116.17 seconds with zero errors and one documented CrowdFollowing initialization warning. It exercises real navigation/gate regeneration, collision/local evidence, pause, four-operative boarding and the height-boundary regression, van destruction, representative active save/load, corruption rejection, escort, extraction/reward consistency, casualties, failure and restart. Background NPC ticks are controlled for isolated contracts; this does not establish natural balance.

Package05 BuildCookRun succeeded in 64.54 seconds. Cooked map, material, both textures, weapon data and input/DPI settings were independently read back. The archive contains 48 files and 956182427 bytes excluding runtime Saved files; it was launched on the documented Windows test system.

Ordinary controls completed the foot mission in Package04. Final Package05 verified 1080p rendering, fresh group boarding without the earlier workaround, vehicle travel/disembarking, and loading/resaving that completed mission with 6000 credits retained once. Actual captures are in Docs/Evidence; the Package05 won frame represents a restored mission, not a new completion.

Final Package05 completed two 10,000-frame captures with process exit 0 and a measured physical 1920 × 1080 client. The second capture contained 9,999 positive samples over 113.05 seconds: mean 11.31 ms (about 88.44 FPS), p95 12.14 ms, maximum 59.71 ms. The 60 FPS average/frame-budget target is met for this idle district on the documented integrated Arc 140V system; heavy combat and full-mission performance remain unmeasured. CSV processing runs synchronously through an engine-supported option to avoid a diagnosed capture-worker teardown fault; profiling overhead is included. Detailed evidence and limits are in PERFORMANCE.json and VERIFICATION.md; reproduction is in BUILD.md.

## Known defects and fidelity gaps

- Procedural geometry and articulated primitive figures remain placeholders. Production models, rigs, full action animation, final materials, lighting/weather, original sound, music, voices and subtitles are pending. This art does not yet meet the original game's presentation benchmark.
- Foreground buildings can obscure controlled operatives, as observed during the vehicle exit check. Roof/floor cutaways, controlled-unit outlines, advanced occlusion handling and floor selection remain pending.
- The van uses bounded swept movement. Autonomous traffic avoidance/rerouting, transit and full vehicle physics are absent. Guards currently ignore seated operatives; vehicle targeting/pursuit is incomplete.
- NPC schedules, suspicion/restricted-area rules, reporting/communications, alarm recovery, roadblocks and coordinated rival teams are incomplete.
- Specialist acquisition uses a contextual escort. Neural settings/override are absent; this is not Hostile Acquisition or proof of its alternate approaches.
- Inventory ammunition/selected equipment persist, but purchasing/drop/retrieve/transfer/sale and recovered-technology research are pending.
- Cybernetics, reserve roster, recovery/recruitment, ironman, corporation customization, territories, taxation, rivals, research, campaign time and subsequent operations are pending.
- Wider weapons, selected structural destruction, difficulty/friendly-fire settings, classic mapping, save-slot UI, scalable accessibility/settings and tutorials are pending.
- One foundation district is present. No polished operation, campaign territory or empty map is counted toward the ten-operation alpha or eventual 50-territory target. Packaged compatibility on a clean second machine remains untested.

## Next bounded step

Begin the Hostile Acquisition vertical slice with a reviewed environment/operative art benchmark at the normal camera distance and reliable controlled-unit occlusion handling. Preserve the original four-operative real-time structure while connecting briefing/intelligence, preparation/modifications, functional neural settings/override, recovery/debriefing, territory income, a researched usable upgrade and a subsequent operation.

All seven brief parts and Milestones 2–4 remain preserved in MASTER_BRIEF.md and FIDELITY_LEDGER.md.
