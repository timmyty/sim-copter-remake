# Baseball team restoration — October 12, 2026

**Later extension:** the user subsequently requested full nine-player rosters and actual games with only active players rendered. [Nearby baseball games](BaseballGames.md) now describes the default behavior. This document records the original eight-player restoration, which remains available through the legacy setting and regression tests.

The original field has **eight people: one batter and seven fielders**, not a nine-person modern baseball lineup. The latest performance log records a transition from DiabloVille to Canyon (`city20.sc2`) at 13:21 CDT. All four Canyon fields reproduced the reported one-player result when the shared spawning budget had one attempt left.

## Confirmed causes and correction

- The shared per-think budget truncated the eight-person spawn. A radius check against any ambient pedestrian then treated the incomplete field as staffed; a passerby could also suppress the whole team.
- The ordinary civilian cap and camera leading-edge scan could skip a field after a warp/load or while the player stayed still.
- The remake chose a random civilian class, whereas original `FUN_004c25b0` passes class **4** for every baseball player. The original uses a separate batter color and one shared fielder color.
- The remake enabled automatic obstacle turning for the batter. Original `FUN_004c7090` initializes that flag to `(state != 0)`, and baseball is state zero. A blocked base run should return to the script rather than bend its heading around a fielder or fence.

Nearby fields now reconcile their roster by the shared building footprint node and baseball program IDs. Existing fielders reserve their nearest authored slots, so missing players can be restored without restarting surviving players. A bounded team spawn finishes despite the shared street-spawn budget; the hard population cap still applies and insufficient capacity defers the team. Actual geometry can reject a spawn point, which is retried on later nearby updates.

New players use class 4, the original eight offsets, and unchanged shipped BHAVs **1205**, **1206**, and **1040**. The batter waits and calls four base-running legs; fielders cycle `HipH`, `WvNo`, and `NoMo`. The baseball-specific automatic-turn correction also applies when restoring old saved teams, without replacing their saved VM stack or changing the save format. Surviving players from old saves retain their existing appearance; newly spawned replacements use the corrected class.

## Proximity and performance

Both field-maintenance and leading-edge spawning require the field center to be within **eight tiles** horizontally of the controlled pawn: the helicopter while flying, the pilot on foot. At the current 400 cm tile scale this is 32 metres. Existing ambient cleanup removes unprotected players beyond **twelve tiles** (48 metres). The gap prevents repeated spawning at the activation boundary. Mission-owned, carried, or injured people retain the existing protected lifecycle.

There are eight players per active field; repeated checks do not duplicate them. Distant baseball teams are not created or simulated by this restoration. Maintenance runs at the existing population-think cadence, not on every rendered frame. No frame-time or FPS improvement is claimed: a live performance comparison has not been performed.

## Validation

The final headless run passed **29/29** tests with no warning-bearing test cases, including:

- All four actual Canyon fields, previously one player each and now eight.
- One remaining spawn attempt, a nearby passerby, partial teams, stationary warp arrival at the civilian cap, and the hard actor cap.
- A possessed helicopter approaching, waiting, leaving, and returning: 0 → 8 → 8 → 0 → 8 players; the leading-edge path also respects distance.
- Actual actor ticks through all four base-running calls and all three original fielder clips; helicopter curiosity cannot take over their scripts.
- Saved baseball VM continuation and repair of the old turn flag, plus existing VM, civilian movement, people-city rules, and save tests.

Editor, Shipping, and Development builds passed. Both installed executables and their symbols were replaced with verified backups and matching SHA-256 hashes; see [the validation receipt](validation/BaseballTeamRestorationResults.json). No interactive gameplay, visual/listening comparison, or FPS benchmark was performed.

Evidence is retained in `Docs/scratchpad/baseball-restoration/`: original executable extracts and BHAV dumps, failing reproduction reports, final automation reports, build logs, source diff, source hashes, and installation backups. The development source folder has no Git metadata; this work does not publish a new release or update the separate publication checkout.
