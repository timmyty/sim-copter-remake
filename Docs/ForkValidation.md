# Fork publication validation — October 8, 2026

Validated with UE 5.8.3 on Windows x64 before publication. The source fork contains
the C++ used for these builds and tests; publication staging was checked against
the working source. This is a source publication, not a new packaged release.

| Check | Result |
| --- | --- |
| Required wrapper, Editor Win64 Development | Succeeded |
| Required wrapper, Win64 Shipping | Succeeded |
| Full `SimCopter.*` headless automation suite | 298 executed: 296 passed, 2 existing failures, 0 unrun |
| Passes with warnings (included in the 296) | 7 |
| Police taser/custody integration test | Passed |
| KINV audio payloads against playlist SHA-256 | All 21 matched |
| Interactive on-screen gameplay/listening | Not performed |
| New cook/package or installed-game replacement | Not performed for this publication |

The complete per-test result list is [ForkAutomationResults.json](validation/ForkAutomationResults.json).
Build warnings include existing Unreal API deprecations. The seven warning-bearing
passes are retained in the full local test report; the suite was not all green.

## Nonlethal police verification

`SimCopter.AirOperations.RecoveryCustodyControlsAndPersistence` creates real Unreal
world actors and runs the passenger camera, firing routes, NPC state and custody
operations. It verifies:

- A living officer is aboard when passenger taser mode starts, and the AI flight
  role activates. Leaving the mode restores manual control.
- Entering the mode clears held and queued helicopter weapon inputs.
- An aimed taser hit stuns the actual criminal, preserves health, leaves them
  alive, does not convert them into a medical patient, and awards no early arrest.
- Ticking the stunned actor keeps its behavior suspended and clears movement; it
  cannot walk or flee. Normal vertical ground settling is still allowed.
- An armed Apache cannot launch bullets or missiles via held fire or direct weapon
  commands while its police taser mode flag is set. The ordinary passenger taser
  also leaves both projectile pools empty.
- A civilian blocks the sightline without injury or arrest. Cooldown prevents
  rapid repeat shots, and controller RT stuns without arming the aircraft weapon.
- The stunned suspect boards alive, becomes handcuffed, completes police delivery
  alive at unchanged health, and retains custody. Existing roof-handoff assertions
  also verify seat release and once-only completion.
- On-foot taser hits likewise preserve health and life; nearby suspects can be
  carried automatically, with walls, floor separation and drop delay respected.

Code inspection confirms both fire functions call `StunForArrest` and display a
cosmetic electrical discharge. They do not launch an Apache projectile or invoke
damage/death interactions. The “sniper” labels and generic crash impact have been
removed from this role. No live visual, controller-feel or sound-mix check is claimed.

## Two previously documented failures

1. `SimCopter.Formats.SimCity2000.ReferenceCity`: the local city fixture lacks three
   exact flat/water/sloped altitude samples expected by this test.
2. `SimCopter.UI.FlapLayout`: its expectation says the four original flaps cover
   all current equipment bits; the old flap mask is 31 versus the current 127.

Both were already recorded in [RescuePilotFixes.md](RescuePilotFixes.md#validation)
before this taser/publication task. Neither test was disabled or rewritten to hide
the failure. This publication changes neither failing test. These are disclosed
limitations, not a claim that every existing defect has been fixed.

## Reproduction and retained evidence

See [ForkBuild.md](ForkBuild.md) for dependencies, build wrapper and test commands.
Full local logs are under `Docs/scratchpad/fork-publication/`; local scratch, backups,
original commercial data, installed builds and NVIDIA SDK files are excluded from
new publication additions. The compact result manifest above travels with the fork.
Earlier feature documents contain historical package/install checks; they are not
new October 8 packaging or playtest claims.
