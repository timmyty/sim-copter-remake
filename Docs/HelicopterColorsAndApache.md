# Helicopter colors and the mystery Apache

Updated October 5, 2026.

All nine helicopters use a dedicated painted material and corrected original-model palette ramps. Red stays red instead of moving toward the yellow fire palette, and blue, teal, purple and olive retain stronger color. Civilian aircraft have a satin finish; the Apache has matte olive paint with dark olive cockpit frames and tail trim. Literal black, navigation lights and the existing cabin glass are preserved. Fine local-coordinate grain changes roughness, fading below pixel size to avoid distant shimmer. Aircraft no longer inherit the shared ground-vehicle metallic value or city surface shading.

The hangar now has a ninth, **Mystery helicopter** card. Before reaching an eligible city, its identity, equipment and price are hidden and purchasing is blocked. Entering an eligible city reveals the Apache. Owning one also keeps its card visible. It uses the normal flight controls, includes missiles and a continuous-fire machine gun, and has no passenger seats. Select a weapon in Tools and use the primary tool action; hold it for the machine gun.

## Five original spawn locations

All five late-game career maps retain their original hidden location. The first `0xE7` XBLD tile in row-major order is used, following original `FUN_0047c0c0`; original `FUN_0047a240` places helicopter type 2 there. The test reads all thirty supplied career cities and confirms these five eligible maps:

| Career file | City | Original tile X, Y |
| --- | --- | --- |
| City25 | Whattheheck | 18, 123 |
| City26 | Four Cities | 23, 46 |
| City27 | Toronto | 37, 61 |
| City28 | Conville | 74, 59 |
| City29 | Metropolis | 103, 48 |

These are zero-based file indices and tile coordinates. Earlier cities and user-city sessions do not reveal or spawn this encounter. Finding and boarding the hidden Apache claims it for free. Alternatively, after the card is revealed, purchase delivers it to a clear airport pad. The price is calculated from current helicopter tuning as **3 × the most expensive civilian helicopter**: currently **61,500 Bucks**, versus the MD Explorer's 20,500.

Successful delivery removes the unclaimed hidden aircraft, preserving the current helicopter and avoiding duplicates. Failed purchases leave it in place. An existing Apache prevents another encounter spawn. A saved per-city encounter flag prevents reloads or selling from generating another free aircraft; it resets on a new career or career-city transition. Existing saves remain compatible, and old saves already containing an Apache are deduplicated.

## Validation and evidence

- Editor and Shipping builds passed through `RebuildUnrealCpp.bat`.
- 43 tests passed across Apache, models, hangar, effects, saves, medical/cabin behavior and flight. New coverage exercises all five map gates, actual original map markers, exact purchase cost, insufficient funds, duplicate prevention, sale/reload state, missile launch/cooldown, held and released machine-gun fire, and all nine procedural models/materials.
- Two existing synthetic-fixture warning groups remain: the effects save test omits an original-game root for its tear-gas mesh, and the hangar parking test omits a world context during actor destruction. All three new Apache tests are warning-free.
- The older `CanExitHelicopter` test was corrected to match the already implemented airborne-exit/fall-damage behavior documented in `AirOperations.md`. Flight production behavior was unchanged by that correction.
- The actual fuselage and cabin sections were exported for an offline studio color review. [Fleet preview](scratchpad/helicopter-colors-apache/fleet-paint-preview.png). This preview uses Blender lighting, excludes rotor assemblies, and is not a gameplay screenshot.
- Cooking and packaging succeeded; the cook reported zero errors and the two existing warnings about an obsolete authored city path. Container inspection confirms the new paint material and both maps. Staging briefly retried its local Zen connection and recovered successfully.
- Installed at `C:\SimCopterRemake\SimCopter Remake v1.0.1\Windows`, with a complete previous-build backup at `Windows-before-helicopter-colors-apache-2026-10-05` beside it. Verified 869 NonUFS manifest entries and all six original-data trees. Eight changed installed files match their package hashes; 45 preserved city/save files and 21 radio tracks match the backup.

No interactive in-game appearance, shop layout or controller check was performed. Build logs, test reports, original C++ baseline, source diff, material bake, offline renders, package and installation evidence are under `Docs/scratchpad/helicopter-colors-apache/`. The material can be regenerated with `Tools/Unreal/CreateHelicopterPaint.py`.
