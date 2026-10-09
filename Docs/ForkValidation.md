# Fork validation

## Combined task publication - October 9, 2026

This run validates the combined cheats/Gort/media, aircraft/road, city-intro/accident,
witness-photo and hospital-medic implementations. The 371 source, configuration,
project and Windows build-wrapper files match the development build after normalizing
line endings. The full per-file hash list is retained locally; its deterministic
tree digest, asset hashes and per-test results are published in
[TaskIntegrationResults.json](validation/TaskIntegrationResults.json).

| Check | Current result |
| --- | --- |
| Required wrapper, Editor Win64 Development | Succeeded after recompiling the media test registration |
| Required wrapper, Win64 Shipping | Succeeded |
| Full NullRHI automation suite | 316 executed: 314 passed (9 with warnings), 2 previously documented failures, 0 unrun |
| Offscreen D3D12 graphics suite | All 4 passed: drive-in playback, original cheat dialog, Gort render and witness photograph |
| Runtime source/configuration correspondence | All 371 files match the publication checkout |
| New runtime assets | Both MP4s and both drive-in material/texture assets match the tested source |
| Existing installed content | Both movies and all 5 cooked content containers match the validated package; required material/maps are present in the cooked inventory |
| Installed Shipping executable | Updated from the validated build with a verified backup; SHA-256 recorded in the result manifest |
| Interactive gameplay/listening | Not performed |
| Public downloadable executable release | Not created; GitHub contains buildable source/assets and the local installed build is updated |

The two remaining failures are unchanged from the initial publication below:
`SimCopter.Formats.SimCity2000.ReferenceCity` lacks the three expected altitude
samples in the local fixture, and `SimCopter.UI.FlapLayout` expects equipment mask
127 where the four original flaps cover 31. Neither test was disabled or rewritten.
Existing toolchain/deprecation and synthetic-world warnings remain.

The first combined NullRHI run also selected the GPU-only movie test, which correctly
rejected that environment. Its registration now uses `NonNullRHI`, consistent with
the existing witness rendering test. The final headless suite excludes both, and
the separate renderer-backed run passes them. Cheat-dialog and Gort previews were
explicitly enabled for the graphics run rather than relying on their headless passes.

The playable installation already contained the prior tasks' cooked media/assets;
those were hash-verified against their package, so a new content cook was unnecessary.
The final executable/symbols and current player/change documentation were installed
with backups. Saves, cities, radio tracks and videos were preserved and hash-checked.
The installation receipt and complete logs are retained locally in
`Docs/scratchpad/task-integration/`. This source publication does not distribute
the user's original-game reference data or external SDK dependencies.

## Initial publication validation - October 8, 2026

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
