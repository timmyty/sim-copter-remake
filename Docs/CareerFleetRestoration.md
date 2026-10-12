# Owned helicopters at the next airport

October 11, 2026

The reported aircraft was a **Dauphin**, corrected from Bell 212. This affected every parked purchase: career ownership survived graduation, but `ApplyPendingAircraftRestores` only recreated the active aircraft. An affected save could consequently retain the owned bit without a parked-aircraft record.

Original `FUN_0047a240` iterates the career ownership mask at `+0x44`, obtains a pad through `FUN_0048b000`, and places each aircraft with `FUN_00484790`. The local decompilations are `.ghidra-exports/0047a240.json` and `0048b000.json`.

After restoring saved aircraft or the selected aircraft's city transfer, the game mode now reconciles all owned helicopter types. Missing types are created unpossessed on clear airport pads with fleet equipment and the source aircraft's tear-gas rounds. Existing aircraft retain their transforms and runtime state. Sold types, support aircraft and the transport plane do not become owned deliveries. Repeating the restore does not duplicate the fleet.

Restoration prefers the eight hangar-adjacent pads and can also use the four airport corners, which accommodates the full nine-model fleet. The shop retains its existing eight-pad delivery policy. Every delivery retains the existing occupied-tile and full-footprint checks. Ownership and cash are never changed by this repair; unavailable parking leaves ownership intact and logs a placement failure.

`SimCopter.Career.OwnedFleetAirportTransfer` buys a Dauphin while the Schweizer remains active, purchases a harness, captures the actual city transfer, replaces the outgoing aircraft with a fresh map pawn, and invokes the production game-mode restore. It checks the missing Dauphin, equipment, cash, pilot selection, repeated restoration, affected save loading, saved fuel/damage/positions, sold aircraft and all nine owned types on non-overlapping airport pads. The unchanged implementation failed five assertions; the correction passes.

Editor, Shipping and Development builds and 55 headless checks passed. The combined checks include the no-harness pilot-level landing correction, save restoration, flight, controller navigation, career progression and hangar purchases. One existing `HangarPurchaseParking` fixture emits three missing-world-context destruction warnings. No interactive gameplay or physical-controller test was performed.

Installed after the user saved and closed the diagnostics game. Both executables and their symbols were backed up, replaced and hash-verified; save files and launcher flags were untouched. Installation receipt and hashes: [CareerFleetRestorationResults.json](validation/CareerFleetRestorationResults.json). This update also includes [pilot-level automatic landing](RemoteTaserAndPilotRecovery.md).
