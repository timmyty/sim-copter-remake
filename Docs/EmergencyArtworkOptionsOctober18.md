# Emergency artwork options and User-map event limit

The persistent **Options → Gameplay → Emergency Buildings** selector offers **Updated** and **Previous remake**. A city reads the selection when it loads. The live city is not rebuilt by opening Options, which preserves active missions, demolished buildings and collision state. Cancel restores the entered selection.

Updated HO209 has balconies only on the entrance-facing recesses, orange balustrades along the three exposed edges of its lower roof terrace, and two separate overhead entrance sections. The central approach remains open. FS211 retains its original low height, apparatus bays, radar dish and two stacks; PO210 retains its stepped entrance and existing remake roof. The legacy branch retains the original face winding/UVs, restores FS211's former exact integer height scaling, and selects the pre-update hospital facade material. Both facade variants use the shared night-light controls.

User-map event creation now has a common ceiling of three live non-base records. Scheduled rolls, direct incidents, medical incidents, recovery and background records all participate. Difficulty and frequency still affect mission content and timing but cannot bypass the ceiling. The spawn clock pauses at capacity, preventing countdown overflow during long sessions. Career behavior is unchanged. A save already above the limit is allowed to finish its existing jobs, avoiding discarded passengers or failure penalties; no new event is created until fewer than three remain.

This is an explicit remake pacing change from the original scheduler at `FUN_004a6e60`; original mission creation is `FUN_004a7a10`. The rendering follows the original building dispatch `FUN_0047c0c0` while preserving the remake's roof artwork.

Automated checks cover both geometry modes and materials in a real generated city, roof/UV preservation, collision beneath both canopies and at the lower-terrace railing, removal of the rear balcony, configuration round trips, all four mission tiers, save/load, incident/forced-roll limits and freed slots. The release also runs the complete automation suite, offscreen rendering, all three build targets, a fresh cook, archive hash verification and isolated packaged startup checks. Final results: [WindowsRelease120Results.json](validation/WindowsRelease120Results.json).

No interactive flight or controller playthrough is claimed. Scratch evidence: `Docs/scratchpad/release-v1.0.20/`.

The final broad check also reproduced a slow police car stopping outside its destination tile on city8. Dispatch routes now finish at the destination tile center, preserving the original tile-based arrival rule. The all-city response check verifies actual arrival at the current service speed.
