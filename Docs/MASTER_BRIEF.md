# PROJECT BLACKGLASS — master development brief

Source: the owner's seven-part instruction. BEGIN was received. Repository publication was subsequently authorized as public under https://github.com/Velamj/BLACKGLASS. The working title is not a cleared commercial brand.

## Part 1 of 7

### Role and objective
Act as the lead game designer, Unreal Engine developer, technical artist, and QA engineer for an original, fully playable PC game.

Create a faithful modern successor to Bullfrog Productions' original 1993 PC game Syndicate. Preserve its recognizable gameplay structure, player perspective, atmosphere, tactical freedom, and interconnected systems while substantially improving graphics, controls, animation, simulation, AI, mission variety, accessibility, and presentation.

Use the 1993 PC game as the fidelity benchmark. Do not reinterpret this project as a first-person shooter, third-person action game, turn-based tactics game, hero shooter, or base-building RTS.

PROJECT BLACKGLASS is a working title, not a cleared commercial brand.

Create original characters, corporations, writing, mission layouts, interface artwork, models, textures, sounds, and music. Do not extract or redistribute the original game's assets, code, maps, dialogue, branding, or recordings.

### Core player experience
The player directs a ruthless corporation competing for territorial dominance. They deploy up to four cybernetically modified operatives into populated urban environments to eliminate rivals, acquire technology, manipulate people, conduct sabotage, and extract valuable targets.

The fundamental loop is:
Choose territory → purchase intelligence → prepare operatives → deploy → execute a real-time operation → extract → resolve consequences → collect revenue → research and upgrade → expand.

Every major system must participate in that loop.

### Platform and workspace
Build a native Windows PC game, offline and single-player first, with mouse and keyboard controls. Do not substitute a browser demo, slideshow, mock interface, or design document for the game.

Use Unreal Engine with C++ for core systems and Blueprints for appropriate presentation and content work. Inspect the actual installed engine, toolchain, project, and editor integrations before choosing implementation details. Do not assume an engine version or claim an editor connection without checking.

Preserve any existing working project and uncommitted changes. Do not upgrade engines, replace architectures, or delete assets unnecessarily.

Create a fidelity ledger documenting:
Original reference system | Retained behavior | Proposed enhancement | Implementation status | Playtest evidence.

Distinguish original mechanics from new additions. Do not quietly omit difficult systems or label placeholders complete.

## Part 2 of 7

### Visual direction
Create an oppressive industrial corporate future with exceptional detail at the normal gameplay camera distance.

Use dense streets, monumental concrete architecture, elevated transit, factories, offices, service alleys, checkpoints, maintenance tunnels, and layered infrastructure. Combine weathered surfaces with immaculate corporate facilities.

Favor charcoal, concrete gray, oxidized metal, muted earth tones, institutional lighting, and selective illuminated signage. Neon should serve the environment rather than cover everything in purple and pink.

Include daylight, overcast conditions, dusk, and night. Preserve strong visibility and silhouette separation in every lighting condition.

Agents should read immediately as dangerous augmented corporate operatives: tailored clothing, long coats, concealed equipment, restrained cybernetics, distinctive posture, and controlled movement. Create original designs.

### Camera
Use a true orthographic isometric camera where practical. Validate lighting, shadows, transparency, and effects in the installed engine. Use a carefully tuned low-perspective alternative only if a demonstrated rendering issue requires it.

Default to a stable classic three-quarter angle. Support smooth panning, edge scrolling, zoom, agent tracking, and optional stepped rotation.

Implement roof cutaways, foreground occlusion handling, floor selection, and agent outlines. These must reveal controlled units without exposing enemies the player has not detected.

Never sacrifice tactical readability for cinematic effects.

### Animation and environment
Implement convincing walking, running, aiming, firing, recoil, impacts, injuries, reloading where applicable, equipment use, boarding, and disembarking.

Add civilian idles, conversations, commuting, panic, hiding, and flight. Include moving traffic and functioning transit.

Use restrained rain, puddles, smoke, muzzle flashes, sparks, impact decals, debris, and local destruction. Provide scalable effects and a reduced-flash option.

Temporary primitive geometry is acceptable during implementation, but it is not final art. The polished slice must contain coherent, properly scaled environments and recognizable animated characters.

### Interface and controls
Provide four persistent operative status panels, health and ammunition, selected equipment, neural-control settings, objectives, alerts, and an interactive minimap.

Use consistent typography, alignment, spacing, iconography, and scalable text. Show the reason an action is unavailable.

Support individual selection, drag selection, select-all, numbered operative shortcuts, queued orders, stop, hold position, weapon switching, contextual interaction, and camera recentering.

Offer modern controls and an optional classic-inspired mapping. UI clicks must never accidentally issue world commands.

Provide optional tactical pause and order queuing. Preserve continuous real-time play as a complete, supported mode.

### Audio
Create original dark ambient and industrial music with restrained combat escalation. Include spatial gunfire, machinery, traffic, transit, footsteps, alarms, and environmental acoustics.

Add concise voiced acknowledgments, security chatter, and mission briefings with subtitles and adjustable frequency. Avoid incessant quips. Do not imitate identifiable actors or reuse original recordings.

Provide separate music, dialogue, effects, and ambience controls, plus accessibility and camera-motion settings.

## Part 3 of 7

### Operatives
Support deployments of one to four operatives and a persistent reserve roster. Operatives must be individually selectable and useful both separately and together.

Build specialization through equipment, modifications, and behavior settings. Do not impose mandatory hero classes that prevent flexible loadouts.

Support formation-aware movement, coordinated attacks, regrouping, independent assignments, equipment transfer, and reliable movement through narrow passages.

### Combat
Implement real-time targeting with meaningful range, accuracy, line of sight, obstruction, armor, ammunition, and weapon behavior.

Use collision-aware shots and projectiles. Effects must correspond to actual hits. Explain blocked shots and invalid targets.

Provide deliberate focus fire, area targeting where appropriate, suppression, ambushes, and environmental advantages. Cover should emerge from geometry and positioning rather than forcing a grid or turn system.

Friendly fire and collateral damage must be consistently modeled and clearly configurable by difficulty.

### Weapons and equipment
Develop distinct roles for:
Sidearms, compact automatic weapons, shotguns, precision rifles, rotary heavy weapons, flamethrowers, energy weapons, fictional electromagnetic launchers, explosives, timed charges, medical equipment, scanners, defensive shields, and neural-control devices.

Weapons need distinct handling, audio, effects, ammunition costs, range, concealability, and tactical drawbacks.

Avoid random color-tier loot and interchangeable statistical reskins.

Use a clear inventory-slot system, initially targeting eight slots per operative, with meaningful weight and carrying capacity. Support purchasing, resupply, equipping, dropping, retrieving, transferring, and selling equipment.

Recovered unfamiliar technology must be able to influence research after extraction.

### Cybernetic modifications
Implement upgradeable brain, eyes, heart, chest, arms, and legs.

Give each category clear mechanical effects:
- Brain: neural processing and control capacity.
- Eyes: detection and targeting.
- Heart: exertion and neural-stimulation endurance.
- Chest: protection and survivability.
- Arms: carrying capacity and weapon handling.
- Legs: mobility and movement efficiency.

Support multiple upgrade generations, visible laboratory presentation, comparison tooltips, installation costs, and meaningful tradeoffs.

Advanced power, heat, or maintenance systems may deepen these decisions, but must not bury the player in repetitive chores.

### Intelligence, perception, and adrenaline
Preserve three live adjustable behavioral controls corresponding to intelligence, perception, and adrenaline.

Make them functional systems rather than decorative bars:
- Intelligence influences autonomous decision quality within selected orders.
- Perception influences awareness and targeting.
- Adrenaline influences movement and reaction performance.

Implement clearly communicated duration, recovery, tolerance, and overuse consequences. Keeping every setting permanently at maximum must not be universally optimal.

Provide individual adjustment, group adjustment, and configurable presets. Display actual effects and changing baselines.

Direct orders remain authoritative. Explain any safety behavior or refusal instead of secretly overriding player commands.

### Casualties
Operatives, installed modifications, and carried equipment have persistent value. Death and lost equipment must matter.

A recoverable incapacitation state may precede death, but dead operatives must not reappear through unexplained free resurrection.

Provide casualty recovery, extraction decisions, replacement recruitment, and optional ironman rules. A destroyed roster must produce a deliberate campaign outcome rather than a broken state.

## Part 4 of 7

### Living urban environments
Build authored urban mission spaces with believable streets, intersections, pedestrian paths, interiors, elevated routes, tunnels, transit stops, and alternate approaches.

The city must function before combat begins. Civilians travel, vehicles follow routes, security patrols, and mission targets pursue understandable schedules.

Use simulation detail appropriate to distance and relevance. Do not spend the entire CPU budget on background pedestrians.

Differentiate enterable buildings from decorative structures. Support reliable navigation between accessible floors and across bridges, stairs, doors, and tunnels.

### Civilians and security
Civilians should notice nearby danger, flee, hide, report incidents, and react to blocked routes.

Security awareness must depend on what an NPC sees, hears, receives over communications, or already knows. Do not grant every enemy instant global knowledge.

Holstered weapons and ordinary behavior should permit movement through some public areas. Drawing weapons, entering restricted areas, attacking people, or visibly using prohibited equipment should increase suspicion.

Holstering a weapon must not erase a witnessed crime.

Escalation should move through investigation, pursuit, alarms, reinforcements, roadblocks, and specialized response. Show what triggered escalation and how it can subside.

### Neural override
Implement an original mind-control device and progression system.

Allow suitable civilians and other targets to become influenced followers. More resistant targets require stronger technology, favorable conditions, or greater control capacity.

Display range, resistance, progress, active connections, capacity, and loss-of-control risks.

Influenced people can provide access, create distractions, accompany extraction, contribute knowledge, or become recruits where appropriate.

Permit group-level follower orders without replacing the central four-operative squad.

Resolve controller death, separation, interrupted signals, hostile interference, target death, and mission transitions consistently. Every mission-critical influenced target needs explicit success and failure handling.

Mind control must offer meaningful alternatives to direct violence.

### Vehicles and transit
Implement commandeerable vehicles with click-to-travel controls, seats, boarding, disembarking, damage, collision, and usable escape routes.

Include civilian cars, vans, security vehicles, and later heavier transports. Add functioning trains or elevated transit where mission design supports them.

Seat ownership, occupants, blocked exits, vehicle destruction, and saved occupancy must remain consistent.

Traffic should avoid obstacles, respond to danger, and reroute when practical. Vehicles must not provide unexplained invulnerability.

### Enemy operatives
Create rival augmented teams using comparable weapons, upgrades, and tactical rules.

Support patrols, investigation, coordinated engagement, flanking, retreat, regrouping, defensive positioning, and attempts to protect or extract important targets.

Difficulty should improve coordination and resource pressure before relying on inflated health.

### Destruction
Implement interactive doors, windows, barriers, street furniture, equipment, and vehicles, plus selected destructible structural sections.

Destruction must affect sightlines, cover, movement, alarms, and costs. Update navigation when routes change.

Use bounded, authored destruction where necessary. Never claim full building destruction when only cosmetic particles exist.

## Part 5 of 7

### Corporate management
Allow the player to name their corporation and executive, choose an original emblem and color scheme, and establish a recognizable identity across menus, agents, and territorial displays.

Create a strategic world map with ownership, available operations, regional income, public stability, security pressure, and rival activity.

The full-game content target is 50 territories with distinct primary operations, supplemented by selected optional and defensive missions. This is the eventual production target, not permission to generate 50 empty or repetitive levels.

### Territorial economy
Territories generate revenue through adjustable taxation or corporate extraction policies.

Higher extraction should create understandable tradeoffs involving unrest, productivity, security spending, sabotage, and possible rebellion.

Missions should affect territorial conditions. Acquiring a region through careful extraction should produce different immediate consequences from devastating its infrastructure.

Provide clear forecasts and accounting. Avoid arbitrary punishment and invisible economic rules.

### Rival corporations
Create several original rival corporations with distinct technology preferences, security doctrines, territorial priorities, and presentation.

Rivals must conduct explainable strategic actions such as reinforcing regions, advancing research, attempting acquisitions, or exploiting instability.

Avoid unrestricted cheating. Establish documented budgets and rules appropriate to difficulty.

### Research
Support funded research into weapons, equipment, cybernetics, and neural-control technology.

Recovered prototypes, captured specialists, and acquired facilities can unlock or accelerate development.

Make research choices legible, consequential, and connected to field operations. Display cost, prerequisites, progress, estimated completion, and resulting capabilities.

Use a documented campaign clock. Income, research, and rival actions must follow consistent time rules. Pause stops simulation; closing the game does not secretly progress it.

Prevent unlimited risk-free advancement through unattended waiting.

### Missions
Support assassination, capture, coercive recruitment, rescue, escort, technology theft, sabotage, infiltration, convoy interception, territorial defense, and elimination of rival teams.

Include purchasable intelligence, optional reconnaissance improvements, alternate entry points, and equipment-relevant planning.

Major missions should usually support at least two substantially different approaches. Examples include direct assault, covert entry, manipulating personnel, exploiting transit, or disabling security infrastructure.

Objectives must be represented as real stateful systems. Provide explicit outcomes for dead targets, destroyed vehicles, lost items, compromised extraction points, and partial completion.

Do not strand the player in an unwinnable mission without an explanation or an abort option.

### Campaign and writing
Use original briefings, corporate propaganda, rival communications, research descriptions, and debriefings with dry, unsettling corporate satire.

Maintain the perspective of an executive directing morally compromised operations. Do not automatically recast the player as a heroic resistance leader.

Build varied industrial, commercial, residential, port, research, and high-security environments rather than reskinning one city.

The finale should test the established tactical and strategic systems through a major operation, not replace them with an unrelated boss fight.

Include tutorial onboarding, difficulty settings, mission replay where appropriate, campaign completion, credits, and a coherent ending.

## Part 6 of 7

### Implementation architecture
Create a maintainable Unreal project with clear ownership of:
- Orders and selection.
- Operative state and modifications.
- Weapons, inventory, and damage.
- Perception and AI.
- Neural control.
- Vehicles and traffic.
- Mission objectives.
- Campaign economy and research.
- Persistence.
- Interface and presentation.

Prefer a small number of coherent modules over elaborate infrastructure that delays playability.

Use data-driven definitions for weapons, upgrades, NPC archetypes, mission objectives, research, and territories. Designers must be able to tune content without rewriting core systems.

Use engine-supported navigation, input, UI, audio, and animation systems appropriate to the installed version. Verify uncertain APIs against official documentation.

Do not invent plugin capabilities or require unavailable paid services.

### Simulation correctness
Make movement, weapon timing, status effects, and campaign progression independent of rendering frame rate.

Use appropriate update frequencies for perception, traffic, distant civilians, and strategic simulation.

Use seeded randomness for reproducible tests where feasible. Do not claim deterministic simulation unless demonstrated.

Maintain one authoritative gameplay state. UI values, world behavior, debriefing results, and saves must agree.

### Save and load
Provide campaign and mid-mission saving, multiple slots, autosaves, and quicksave outside restricted modes.

Use stable entity identifiers and versioned save data.

Persist operatives, injuries, inventory, modifications, neural settings, influenced targets, vehicle occupants, relevant destruction, objectives, alarms, campaign finances, research, territory ownership, and relevant simulation time.

Restore relationships safely after loading. Do not serialize fragile runtime pointers as persistent identity.

Use safe writes, retain a previous valid save where practical, and handle missing or incompatible data with a clear error.

### Performance
Initial engineering target: 60 FPS at 1080p on a documented midrange Windows test system.

Treat this as a target until measured. Record hardware, resolution, quality settings, actor counts, frame-time results, and major CPU/GPU bottlenecks.

Provide scalable crowd density, shadows, reflections, effects, and view distance. Gameplay-critical actors and information must remain consistent across settings.

Use profiling evidence before introducing complicated optimization systems.

### Assets
Use original assets or properly licensed assets actually available to the project. Maintain an asset manifest identifying source, license, modifications, and placeholder status.

Do not purchase assets, accept paid subscriptions, upload private project files, or publish builds without authorization.

Keep development playable when final art is missing, but document the gap and its replacement path. Never present a mannequin or graybox as completed production art.

### Project deliverables
Maintain source code, project files, playable maps, data definitions, build instructions, controls, tests, asset manifest, fidelity ledger, and a concise development status document.

Provide a packaged Windows executable when the required Windows toolchain is available.

If packaging is blocked, identify the exact missing dependency. Do not fabricate a successful build or imply that editor play proves packaged compatibility.

## Part 7 of 7

### Delivery sequence
Preserve the full-game objective while working in bounded, testable milestones. Keep the project runnable between milestones.

### Milestone 1: playable foundation
Inspect the workspace and toolchain, then implement:
- An isometric urban block.
- Four selectable, animated operatives.
- Individual and group movement.
- Functional navigation and camera controls.
- At least two distinct weapons.
- Civilians and a basic security response.
- One usable vehicle.
- A real objective, extraction, failure, restart, and save/load.

This milestone must be playable through ordinary controls. It is a foundation, not the finished game.

### Milestone 2: polished vertical slice
Create an original 15–25 minute mission, working title "Hostile Acquisition."

The player must acquire and extract a research specialist from a functioning corporate district.

Include public streets, a controlled facility, an alternate service route, traffic, security patrols, an extraction vehicle, and a rival intervention.

Support at least two complete solutions: one emphasizing force and one emphasizing infiltration or neural manipulation.

Connect the mission to:
- A real briefing and intelligence purchase.
- Operative preparation and modifications.
- Functional neural settings.
- Neural override.
- Persistent equipment recovery.
- A debriefing with casualties and costs.
- Territorial acquisition and income.
- Research that unlocks a usable upgrade.
- A subsequent playable operation.

The district must have coherent art, sound, animation, lighting, and readable interface presentation.

### Milestone 3: campaign alpha
Expand to several distinct environments and at least ten substantive operations. Complete the remaining systemic features, rival strategy, progression, mission variety, tutorials, difficulty balancing, and campaign persistence.

### Milestone 4: full campaign and release candidate
Expand toward the 50-territory content target with authored variation. Finish remaining art, voice, audio, accessibility, optimization, endings, and packaged-build testing.

Never count empty maps, reskinned objectives, or unimplemented interface entries as completed content.

### Acceptance tests
Demonstrate that:
1. Four operatives obey individual and group orders.
2. Movement handles doors, traffic, narrow passages, and accessible vertical routes.
3. Obstructions affect shooting correctly.
4. Civilians and security react to actual events.
5. Neural controls measurably change behavior.
6. Neural override supports mission completion.
7. Boarding, movement, destruction, and exit states remain consistent.
8. Equipment, casualties, rewards, and research persist correctly.
9. Save/load restores a representative active mission without duplicating entities or rewards.
10. A mission can be completed through its supported alternate approaches.
11. Campaign progression changes subsequent play.
12. Performance and packaging claims are backed by actual tests.

### Reporting
After each milestone report:
- What is playable.
- Exact project/map/build paths.
- Controls and launch instructions.
- Tests actually run and their results.
- Screenshots captured from the running game.
- Known defects and placeholders.
- Remaining fidelity gaps.
- The next bounded implementation step.

Never substitute concept art, generated screenshots, or source-code inspection for proof of working gameplay.

### BEGIN
All seven parts are provided. Inspect the real environment, establish the smallest safe implementation plan, and build Milestone 1.

Proceed beyond planning into implementation. Stop at a tested milestone or a genuine blocker, with exact evidence and the next action. Preserve all remaining requirements for subsequent passes.
