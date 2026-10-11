# Dispatch transport clearings and patient completion

Transport still starts with the original mission-building selection from `FUN_004a7a10` and
`FUN_004a92f0`. Each endpoint then searches at most 3,200 cm (eight default city tiles) for the
nearest ground clearing. It requires a 275 cm half-width footprint, level dry terrain, no
occupied building cells or rendered building bounds, and an unobstructed vertical approach.
Roads can qualify; bridges, roofs, overhangs, trees and steep/wet edges cannot. A new call without
local usable endpoints is rejected instead of moving it across the city. Existing saved drop-off
coordinates are resolved on restoration without changing passenger identities or mission counts.

The helper searches with at least that clearance, enlarged to fit its actual rotor/tail bounds.
The measured Jet Ranger footprint is X -173.84..154.10, Y -154.26..153.96 cm;
275 cm accommodates its worst-heading corner radius plus a 24 cm margin. The first
350 cm trial rejected all destinations in two cities and was corrected from these measurements.
For transport pickups it holds a reachable clearing and guides the same person there at the
existing passenger walking speed. The walking route must be dry, nearly level and unobstructed;
boarding still requires physical proximity. The helper no longer targets the centre of a
destination roof. A loaded recall uses the passenger's event to finish delivery before returning.

The previous helper delivery shortcut called `SetMissionRetiredAlivePose` and imposed an
eight-second lifespan. Living patients now resume the post-delivery portion of original
**BHAV 282**, beginning at record 6: wait until released, choose the `Whoa` animation and
voice event 6 or 59, run `Idle-20`, and then follow the original retirement/despawn branch.
The original graph is recorded in
`Docs/scratchpad/agent-sessions/2026-08-05-ambulance-and-dash/medic_bhavs.txt`.
Healthy transport passengers retain this fork's existing behavior of becoming ambient walkers.
The normal medic handoff continues to own its original behavior graph.

Regression coverage includes cramped ground pickup, a blocking destination building, loaded
recall, duplicate delivery prevention, patient animation completion, bounded search, water,
saved destination stability, and placement samples from every bundled city. Evidence and
pre-change source are in `Docs/scratchpad/transport-clearings/`.

Editor and Windows Shipping builds succeeded. All 105 selected headless checks passed, including all 45 bundled cities; 2 tests reported fixture warnings. The installed executable and symbols match the build by SHA-256, with verified rollback backups and updated manifest timestamps. No interactive playtest was performed. [Machine-readable results](validation/TransportClearingsResults.json).
