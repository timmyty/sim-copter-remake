# Rescue, pilot equipment, passenger exits and catalog liveries

Implemented 2026-10-05 at the user's request. These changes intentionally extend the original game's behavior where requested.

## Gameplay changes

- Shipwreck survivors spawn on the actual capsized-hull triangles. Their local footholds follow translation, yaw, pitch and roll. Walking and pushes are constrained to supported points; water animation is disabled while on the hull. Boarding releases this constraint, and loading a save reconnects the surviving people to their hull. Occupied rescue hulls cannot be towed until their people have been rescued.
- New rescue groups draw from six civilian classes with distinct portraits; a new event starts a new shuffled bag, including callers that spawn one person at a time. A restored cabin refreshes portrait identity and mood from its actual restored passenger. The reported all-female group did not reproduce in the source baseline's fresh five-person spawn/board test; the prior source already had a larger figure bag, which repeated some male portraits. Existing saved people retain their identities.
- Ordinary exits search both sides and several body-sized positions. Visible people, collision and blocked paths disqualify a position. If all positions are blocked, the passenger stays aboard and the behavior program retries. Mission delivery, helper-aircraft unloading and manual ground drops also respect this reservation; a failed exit cannot be consumed by the legacy pointerless-seat fallback. Police roof handoffs wait for their receiving point to clear. A direct medic handoff and deliberate airborne drop retain their separate behavior.
- Exiting an airborne helicopter shuts off its engine and enters the existing dying-aircraft descent, impact and recovery path, including the established passenger write-off rule. Zero controls previously left the original flight model hovering.
- **P / controller B** toggles the pilot's parachute while falling. A visible orange/cream canopy, suspension lines and pack appear, with a wider third-person camera. The canopy reduces gravity and gradually brakes descent. It must slow the pilot before impact to prevent fall injury; deploying at the last moment does not erase damage. Closing it starts a new unprotected fall. Landing stows it. On-foot save payload version 3 stores deployment and still reads versions 1 and 2.
- The taser is generated 3D equipment with a yellow receiver, black grip/cartridge, open trigger guard, trigger, sights, safety switches and two electrodes. Its effect originates at the equipment rather than at the camera. Existing aim/fire controls remain right mouse/left mouse or LT/RT.
- Vehicle towing entries say **Stalled vehicle**, including world markers; hull towing markers say **Boat recovery**. The generic marker fallback uses the mission type's display name.
- The spotlight camera sits below the banked fuselage bounds and excludes its own helicopter from the pilot's view. Changing camera mode or leaving the helicopter restores aircraft visibility.

## Catalog comparison

Compared the eight original catalog bitmaps to the actual generated fuselage sections. The GEO palette selectors describe paint regions but their default colors differ from the catalog drawings. `MakePaintPalette` now supplies model-specific colors; the common model builder passes the type through. Tires/windows and navigation-light palette entries remain unchanged, and equipment/rotors are built separately.

| Aircraft | Catalog image | Paint scheme |
| --- | --- | --- |
| Schweizer 300 | CAT_SCHW.BMP | Tan frame/tail, green tubular structure |
| Jet Ranger | CAT_JET.BMP | Blue roof/tail, silver cabin, burgundy lower hull |
| MD 500 / Hughes 500 | CAT_HUGH.BMP | Silver/gray, red engine accents and skids |
| MD 520N | CAT_MD5.BMP | Royal blue upper body/tail, silver-white cabin |
| Bell 212 | CAT_BELL.BMP | Teal trim, silver side panels |
| Agusta A109A | CAT_AUG.BMP | White cabin, dark slate blue-gray engine housing, belly and tail |
| Dauphin 2 | CAT_DAUP.BMP | Gold upper/lower hull and tail, white cabin band |
| MD Explorer | CAT_MDE.BMP | Navy body/boom, thin white separator, red belly/tail accents |

Apache has no original civilian catalog drawing; its existing dark olive military paint is retained. Some catalog accents need different regions from the default GEO livery: Jet Ranger/Dauphin belly bands and Explorer's white/red stripe split existing polygons without changing their surface. MD 500 skids and engine panels use the red accent. These are paint-scheme matches, not pixel-identical copies of the pre-rendered catalog illustrations; original geometry, scene lighting and material response still affect appearance.

![Actual game fuselages rendered offline](scratchpad/rescue-pilot-fixes/fleet-review.png)

## Validation

- Editor and Windows Shipping builds use `RebuildUnrealCpp.bat`.
- Final focused suite: **53 passed, 0 failed**, including real procedural boat surfaces through motion/tilt, hull save reconnect, passenger identity/seat restore, distinct exits, blocked exit retry, blocked manual/mission releases, actual camera selection/visibility, crash descent through impact, actual CharacterMovement falls with no/early/late parachute, canopy save/restore, equipment visibility and mission names.
- Full suite before the final paint-band and helper-exit refinements: **292 passed, 2 failed**. `SimCopter.Formats.SimCity2000.ReferenceCity` assumes three specific altitude samples absent from the supplied career city. `SimCopter.UI.FlapLayout` assumes the four original flaps cover all seven current equipment bits (31 versus 127). Both tests and their implementation dependencies are byte-identical to the pre-change snapshot; recorded hashes are in `unchanged-test-dependencies.json`. No test was disabled.
- Actual fuselage/equipment sections exported from Unreal were rendered in Blender and visually inspected. These images are offline geometry/color reviews, not screenshots of the running game.
- No interactive gameplay session was launched. Final appearance, animation and camera feel in a live city remain a human visual check, per AGENTS.md.

Build/test/cook/package/installation evidence, source snapshot/diff, exports and render scripts: `Docs/scratchpad/rescue-pilot-fixes/`. See `installation.json` for the installed executable, full previous-installation backup and verified file hashes.

Installed in `C:\SimCopterRemake\SimCopter Remake v1.0.1\Windows`. Full backup: `Windows-before-rescue-pilot-fixes-2026-10-05`. Installation verified 869 NonUFS files, replaced 7 changed files, and preserved 45 local city/save files and 21 custom radio tracks.
