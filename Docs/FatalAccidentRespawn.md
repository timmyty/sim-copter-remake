# Fatal accident respawn — October 14, 2026

After the existing five-second fatal-fall delay, the player now returns directly
to their helicopter, with possession, camera and cockpit controls restored. A
surviving aircraft stays at its current airborne or grounded location and keeps
its fuel, damage and engine state. A destroyed aircraft uses the existing crash
repair and airport-pad placement path, which avoids occupied pads. This is a
requested remake behavior change; airport recovery continues to use the port of
`FUN_0048a8b0` and its existing no-available-pad fallback.

Previously, fatal recovery called `ResetAircraft` even on a surviving aircraft,
then teleported the pilot beside it without boarding. That also refilled fuel,
repaired damage and stopped the engine. Contact boarding and automatic pickup
could independently interrupt the death delay.

Fatal recovery now owns that transition: contact boarding is blocked while dead,
and an active recovery autopilot holds the surviving helicopter during the
countdown. The pilot then enters through the normal helicopter possession path
without a camera blend from the accident location. The old pilot loses collision
and ticking, is hidden and is scheduled for cleanup. Carried people are put down
before respawn. A countdown that expires without a controller remains pending
until possession is restored. The autopilot preference and save format are unchanged.

Implementation is in `SimCopterOnFootAirOperations.cpp`, `SimCopterOnFootPawn.cpp`
and `SimCopterPilotRecovery.cpp`. `SimCopterFatalRespawnTests.cpp` covers the actual
exit, fatal landing, delay, possession and camera transition with autopilot on and
off, a grounded aircraft, a destroyed aircraft with the first airport pad occupied,
a saved fatal fall, an expired countdown without a controller, and a nonfatal fall.

All six fatal scenarios failed against the original runtime; the nonfatal case
passed. After the fix, all seven new cases and eleven related gameplay cases pass
without warnings. The headless report records 19 successes because the existing
`SimCopter.AirOperations.TaserVisuals` test returns immediately under NullRHI; it
does not count as visual verification. The camera fixture was corrected to create
and initialize a real player camera manager before the final run.

Editor, Shipping and Development builds succeeded. Both installed executables
and their matching symbols were updated with verified backups and SHA-256 hashes.
No assets or save schemas changed, and no Inspector or project setup is needed.
The publication checkout was not modified. Build logs, before-change sources,
test reports and the installation receipt are in `Docs/scratchpad/fatal-respawn/`;
the compact result is [FatalAccidentRespawnResults.json](validation/FatalAccidentRespawnResults.json).

No interactive playtest was performed. To check the presentation manually, jump
from above 18 metres onto dry ground without a parachute, wait five seconds, and
confirm immediate control of the surviving helicopter. Repeat with autopilot off
and after the helicopter has crashed back to the airport. A nonfatal hard landing
should leave the pilot on foot.
