# Development status

BLACKGLASS is a native, offline Windows real-time isometric tactics project. Its working title is not a cleared commercial brand.

**Milestone 1 remains a playable foundation. The new graphics pass compiles, has passed its native integration scenario, and is packaged and rendered in Unreal 5.8.3. Art remains interim work; the polished vertical slice and full campaign are not complete.**

## Current graphics pass

- Industrial facades now have warehouse, administration and research identities, framed windows, structural bays, roof parapets, skylights, equipment, pipes, signage and paving detail. Visual dressing does not add collision or claim structural destruction.
- Thirteen original source meshes replace cube characters/van panels with shaped coats, heads, limbs, boots, a van cabin/body/tires and thin selection rings. The sources total 5,584 triangles; character parts remain articulated static meshes, with final skeletal rigs and action animation pending.
- Original concrete/asphalt wear and normal textures drive an owned industrial material. Original foundation assets are preserved. Instanced-static-mesh support is saved explicitly.
- The true orthographic camera uses the classic 35.264-degree isometric pitch and closer default framing. Living on-foot controlled operatives use role-filtered silhouette outlines; NPCs are excluded. Projected head-to-foot selection helps pick obscured operatives.
- Four persistent status panels, compact operative labels, alerts and the minimap use a cleaner layout. Foreground roof/floor cutaways and floor selection remain absent.

## Playable foundation

Four independently selectable operatives support individual/group/queued real-time orders, stop/hold, two distinct weapons, reloads and holstering. Navigation covers streets, two interactive/destructible gates, a narrow service route and a raised bridge with ramps. Shots use real collision, range, ammunition and damage. Eight civilians follow simple routes; four initial guards use local sight/sound and remembered incidents, with bounded reinforcement response.

A six-seat van supports approach/boarding, driver orders, swept travel, blocked exits, damage/destruction and saved occupants. Acquire Iona Vale contextually, then extract her and every survivor for one 6,000-credit reward. Target loss, squad loss, abort and restart are explicit. Optional tactical pause and versioned F5/F9 quicksave/load preserve implemented authoritative state; safe writes retain a previous valid save.

## Verification and current blocker

The native integration scenario passed in 115.85 seconds with zero errors and the existing documented navigation initialization warning. It covers movement/gates/vertical routes, shooting/local evidence, pause, occupancy/destruction, representative save/load, casualties, extraction/reward-once and restart. Nine outline-state checkpoints exclude NPCs and verify selection, vehicles, casualties and restoration through normal ticks. The final cloth/selection-ring refinement and corrected sign placement are included in this passing run.

Final Editor build04 succeeded in 7.80 seconds. Package07 BuildCookRun succeeded with exit 0 in 42.91 seconds; its archive contains 48 files/957,441,458 bytes excluding runtime Saved files. Its native executable launched with a measured physical 1920×1080 client. Cooked listing confirms the map, 13 meshes, 2 new materials and 4 new textures. [Actual packaged render](Evidence/package07-district.png) was captured directly from Unreal's game viewport, with 32 render warmup frames and no image editing.

**The Windows test desktop's active screensaver currently prevents foreground access.** The input helper rejected focus with foreground PID 0 and sent no input. New physical-control/occluded-selection, boarded/dead-outline, 720p/rotated-view checks and a representative visible-window benchmark remain pending. Engine viewport captures prove the displayed render; they do not replace those interaction tests. Package05's 88.44 FPS result remains historical in PERFORMANCE.json and does not apply to Package07.

## Known defects and fidelity gaps

- The district remains an authored foundation assembled from procedural shapes. Repeated roof forms, thin details, dark small silhouettes and limited surface/lighting variation still need production art work. The current pass does not establish the original-game visual benchmark.
- Character rigs, polished walking/aiming/reload/injury/equipment/boarding animation, weather, original sound/music/voices/subtitles and a final interface are pending.
- Roof/floor cutaways and actual visual occlusion-state review are pending. Selection silhouettes are implemented but not fully tested through physical controls.
- Van movement is bounded and swept; traffic avoidance/rerouting, transit, full vehicle physics and guard pursuit/attacks against seated operatives are incomplete.
- Full NPC schedules, suspicion/restricted areas, reporting/communications, alarm recovery, roadblocks and coordinated rival teams remain incomplete.
- Specialist acquisition is contextual escort, not neural override. Neural controls/override, cybernetics, reserve roster, casualty recovery/recruitment and ironman remain absent.
- Inventory purchase/drop/retrieval/transfer/sale, recovered-technology research, corporation customization, territories/economy/taxation, rivals, research/campaign time and a subsequent operation remain absent.
- Wider equipment, structural destruction, difficulty/friendly-fire settings, classic mapping/remapping, save-slot UI, accessibility/settings and tutorials remain pending.
- One foundation district is present. No polished operation or empty map is counted toward the alpha/50-territory target; clean second-machine packaging remains untested.

## Next bounded step

Unlock the Windows test desktop, review this packaged graphics pass through normal controls and complete occlusion/boarding/death/save-load, 720p/rotation and visible-window performance checks. Then improve authored environment variation and replace interim character rigs before connecting the Hostile Acquisition systems.

All seven brief parts and Milestones 2–4 remain preserved in MASTER_BRIEF.md and FIDELITY_LEDGER.md.
