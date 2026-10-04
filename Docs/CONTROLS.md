# Foundation controls

These bindings are implemented for the native Windows foundation in `BLACKGLASS.uproject`, map `/Game/Maps/DepotBlock`. Earlier Packages04/05 received the ordinary-input checks recorded in [VERIFICATION.md](VERIFICATION.md). The current graphics pass preserves those bindings, but its new art and through-building selection require a fresh ordinary-input review. Headless integration checks verify authoritative state and do not verify every physical binding.

Launch with [BUILD.md](BUILD.md). The squad is selected and centered once when a new operation starts. Later deselection and loaded selections are preserved.

## Selection and orders

| Input | Action |
|---|---|
| Left-click an operative | Select that operative. |
| Left-drag in the world | Select living operatives inside the screen rectangle. |
| Shift + left-click or drag | Add operatives to the current selection. |
| 1, 2, 3, 4 | Select the corresponding operative; Shift adds them. |
| Space | Select all living deployed operatives, including seated operatives. |
| Left-click an operative status panel | Select that operative; Shift adds them. |
| Right-click accessible ground | Move selected operatives in a small formation. |
| Shift + right-click | Append an order to each selected operative's queue. |
| Right-click a detected guard | Attack that guard with selected operatives. |
| Right-click the specialist | Approach and acquire the specialist as an escort. |
| Right-click an intact door | Send the nearest eligible selected operative to toggle it. Only one operative receives this interaction, preventing group toggles. |
| Right-click the usable van | Approach and board selected operatives. |
| Ctrl + right-click a visible unit, intact door or usable vehicle | Force an attack on that actual entity. Shift can append the attack. |
| X | Stop selected operative orders; stop the vehicle when its driver is selected. |
| B | Stop and hold selected operatives in place; stop a selected driver's vehicle. |

Undetected NPCs are ignored by command picking, allowing ground movement without revealing hidden enemies. This does not change shot collision or physical obstacles. Invalid force-attack clicks share a generic notice. Ground movement requires a valid navigation destination; blocked destinations and unavailable actions report a reason.

HUD clicks are consumed before world commands. Clicking a panel or minimap does not issue an accidental movement or attack order.

## Weapons and vehicle

| Input | Action |
|---|---|
| H | Toggle the selected group's weapons between holstered and drawn. Holstering does not erase witnessed incidents. |
| V | Switch each selected operative's equipped weapon. |
| R | Reload the selected operatives where ammunition permits. |
| E | Board selected operatives who are on foot; disembark selected occupants from a stopped vehicle. |
| Right-click ground with the driver selected | Command the occupied vehicle to travel to that destination. |

The van has six seats; an operative takes the driver seat first. Select its driver through the numbered shortcut or status panel. Stop with X before disembarking. Exits require an accessible, unobstructed position; when exit placement fails, the occupant retains their seat.

Personal weapons cannot be fired while seated. The foundation has no mounted vehicle weapons or passenger firing control.

## Camera and tactical pause

| Input | Action |
|---|---|
| W, A, S, D | Pan the isometric camera. |
| Cursor at a viewport edge | Scroll the camera. |
| Mouse wheel | Zoom smoothly within the foundation limits. |
| Q | Rotate the view by a quarter turn. |
| F | Recenter on the selected operatives. |
| T | Toggle tracking of the selected group. Manual panning disables tracking. |
| Left-click the minimap interior | Move the camera to that district location. |
| P | Toggle tactical pause. |

The default camera is truly orthographic at the classic 35.264-degree isometric pitch. Living controlled operatives on foot receive silhouette outlines through foreground geometry; selected operatives are brighter. Click near the visible head-to-foot silhouette to select an obscured operative. This outline does not expose NPCs or replace missing roof/floor cutaways. Selection, camera controls, orders and saving remain available while paused. Orders execute when simulation resumes.

**Disembarking requires real-time simulation:** release tactical pause with P, then press E again. Boarding orders may be issued while paused and execute after resuming. Pausing also stops the mission timer and autosave interval; closing the game does not advance this operation.

The minimap marks the extraction area, friendly operatives, the van, and detected NPCs. Enemy markers depend on detection. Roof cutaways and accessible floor-selection controls are not implemented in this foundation.

## Saves, outcomes and restart

| Input | Action |
|---|---|
| F5 | Save the current operation to the Quick slot. |
| F9 | Load the Quick slot. |
| F8 | Restart the foundation operation; tactical pause is released. |
| F7 | Abort an active operation without awarding acquisition revenue. |

The foundation autosaves every 120 seconds of active simulation and when success or failure resolves. Native saves are versioned, validate entity relationships and retain a previous valid copy when replacing a slot. Quicksave and autosave are distinct slots.

The native persistence API also supports Slot1-Slot3, but a player-facing multiple-slot menu is not present. The portable C++ fixture's `.bgcore` files are a separate test format and cannot be loaded by F9.

Acquire the living specialist and bring **every surviving operative plus the specialist** to the marked northwest extraction area. The HUD reports how many surviving operatives have reached it and whether the specialist is at the marker. Success settles the 6,000-credit reward once. Losing the specialist or all operatives fails the operation; F8 restarts and F9 can restore a valid saved state. Abort remains available during an active mission.

## Scope and placeholders

The foundation includes four operatives, two weapon roles, an escort objective, civilians, basic security, doors and one usable van. Character animation and district geometry use documented prototype art.

There is no campaign preparation interface, research menu, cybernetic installation, live intelligence/perception/adrenaline controls, neural override, inventory-transfer UI, classic control preset, remapping menu or full accessibility/settings interface yet. Do not infer those features from these foundation bindings. Remaining work is tracked in [FIDELITY_LEDGER.md](FIDELITY_LEDGER.md) and [STATUS.md](STATUS.md).
