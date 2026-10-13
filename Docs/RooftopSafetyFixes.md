# Rooftop exit and rescue safety

Implemented October 12, 2026 in the local playable source.

The reported helicopter exit could choose the door beyond the roof edge. Its downward probe (and the pilot's initial ground snap) could then select the street. Rooftop rescue containment used a square and a 100 cm fall tolerance; a lower setback inside that square could leave a person standing partway down the building. A synthetic collision fixture reproduced nine failed assertions against the original implementation, covering both problems.

Landed pilot exits now try the remembered side, the opposite side, and small fore/aft alternatives. Each position requires support beneath the full capsule footprint near the aircraft's landing height, a clear capsule and a clear door path. If none fits, the pilot stays aboard with possession, audio and HUD unchanged. Deferred spawning binds the parked aircraft before pilot initialization, preserving the selected roof height and deliberate airborne exits. Placement uses the pilot's actual scaled capsule height.

Posted rooftop people now check actual walkable geometry at their assigned roof height as well as the existing square. Both behavior steps and frame movement reject unsupported edges. A stranded person returns to a verified previous foothold or the nearest supported point found within their roof post. The search also repairs saved stragglers without a cached foothold. Ground hospital entrance crews retain their existing patrol behavior; boarding and demolition still release the rooftop post. No save format changed.

These are adaptations to the remake's rendered geometry. The original `FUN_004c9470` tests cell heights; it does not supply an exact footprint for a stepped mesh. The original sources were inspected locally in `.ghidra-exports/004c9470.json` and `004c6360.json`.

## Validation

- Editor, Windows Shipping and Development compilation succeeded.
- All **41 focused headless tests passed**: 40 clean and one existing city-fixture warning case. Includes all nine real helicopter models with both preferred sides and rotated edge landings, no-safe-exit refusal, airborne initialization, mission/service roof confinement, saved-straggler repair, original passenger boarding and exits, wall collision, knockdown, pilot recovery and building destruction.
- The original screenshot's exact city/save was not replayed. No interactive gameplay session was launched, per repository instructions.
- Build logs, source snapshots/diff, regression reports and installation backups are in `Docs/scratchpad/rooftop-safety/`. The final machine-readable receipt is `Docs/validation/RooftopSafetyResults.json`.
- Installed both game executables and their symbols into `C:/SimCopterRemake/SimCopter Remake v1.0.1/Windows`, with verified backups and matching SHA-256 hashes. No cooked assets, saves, custom media or publication checkout changed.

## Manual check

Land near an outer roof edge or setback and press F/Y with the preferred door over the edge. The pilot should appear on the supported side at roof height; if neither side fits, reposition the helicopter before exiting. Let rescue people approach, wave and board near the edge, and load a save with a person already on the facade. They should remain on, or return to, a supported point on their assigned roof. Deliberate airborne exit and building demolition should continue to allow falls.
