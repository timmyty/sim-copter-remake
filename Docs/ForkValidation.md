# Fork validation

## October 12 release v1.0.8

Editor, Shipping and Development builds succeeded. 461/463 headless cases passed after correcting and rerunning an outdated rescue landing fixture that never advanced the falling pilot. The broad run initially passed 460/463; the two previously documented ReferenceCity fixture and FlapLayout mask expectation failures remain. All 8 focused D3D12 offscreen cases passed, and the four README captures were inspected. The staged launcher and its Shipping child survived a hidden 31-second startup with an isolated profile. All 427 source/configuration/project/build/launcher files match the publication checkout. No interactive gameplay, physical-controller or listening check was performed.

Fresh distribution staging verifies 915 runtime files and 903 NonUFS dependencies, including 45 maps, 21 KINV tracks, two movies and 30 flyover soundtracks. The normal Shipping launcher and optional Development GPU diagnostic launcher are included. Personal data and symbols are excluded. Cooked content is unchanged; no recook was needed. [Detailed results](validation/WindowsRelease108Results.json) and [release notes](Releases/v1.0.8.md).

## October 11 release v1.0.7

Fresh combined validation: 437/439 headless cases passed after rerunning two persistence checks with config writes enabled; the two previously documented ReferenceCity fixture and FlapLayout mask expectations remain. The initial broad run passed 435/439: its Multiprocess flag suppressed disk config writes, causing the two additional persistence failures. Engine ConfigCacheIni.cpp confirms that MultiprocessSaveConfig enables those writes; both isolated reruns passed. All 8 focused offscreen tests passed. Both build wrappers succeeded, and the staged launcher survived a hidden 31-second isolated startup. All 403 source/configuration/project/build files match the publication checkout. No interactive gameplay or listening check was performed.

Runtime inventory verifies 912 files and 903 NonUFS dependencies, including all 45 maps, 21 KINV tracks, two movies and 30 flyover soundtracks. Personal data and debug symbols are excluded. [Detailed results](validation/WindowsRelease107Results.json) and [release notes](Releases/v1.0.7.md).

## Windows download v1.0.6 - October 10, 2026

The downloadable package now includes all completed changes through the NPC
identity and medic-care update, plus an expanded README describing this fork.
Fresh distribution staging verified 912 runtime files and all 903 NonUFS
dependencies, 45 maps, 21 KINV tracks, two movies and 30 flyover soundtracks.
The executable matches the successful Shipping build by SHA-256. The launcher
and its Shipping child survived a 31-second hidden startup check with an
isolated profile and no renderer or audio. Personal data and symbols are excluded.

Archive publication requires per-entry SHA-256 verification and matching GitHub
asset digests. This packaging-only update reuses the existing tested executable
and cooked content; it does not claim a new build, recook, full regression run,
interactive playtest or listening check. See [release notes](Releases/v1.0.6.md)
and [package evidence](validation/WindowsRelease106Results.json).

## October 10 NPC appearance publication

Editor and Shipping builds succeeded for the NPC appearance and medic portrait
changes. All **18 focused tests** and the **offscreen character/cabin test**
passed. The render fixture verified ten character comparisons and 4,638 visible
occupant pixels through the helicopter canopy; the resulting images and care
frames were reviewed. No interactive gameplay check was performed.

Before publication, all 4,619 tracked or publication-eligible files were compared
between the development and publication copies, including Git LFS asset object
hashes. No implementation, asset, tool or documentation was missing. The sole
intentional copy difference is the publication-specific MP4 Git LFS rule.
All 393 source/configuration, project and build-wrapper files match; their
normalized SHA-256 is
`0d46a850c8b38fca5904f3ad88a0b563e28dbabd07de0bbf5f3889c9e59666a4`.
The changed NPC source hashes still match the tested files, and the installed
executable and symbols match the successful Shipping build.

The other recent tasks were already published in `29085c5` and its ancestors.
The earlier combined suite below is historical evidence; it was not rerun for
this publication-only audit. Its two known failures remain documented. This
commit publishes source and does not replace the downloadable release archive.
See [NPC results and publication audit](validation/NpcAppearanceResults.json).

## October 10 scheduled publication integration

This is fresh validation of the combined completed source since `754b434`, run
at the authorized 8 PM CDT publication time. All 391 source, configuration,
project and build-wrapper files match between the development and publication
copies. Their normalized SHA-256 is
`c60802ca72e11e38a877010a1873cff99948ec50068c8337aec05f325d6f5250`.

- Both Editor and Shipping wrapper invocations succeeded with zero build actions:
  the current targets were already up to date. This is not a clean rebuild.
- The full headless suite passed **377 of 379 tests** (356 clean, 21 with warnings).
  The same two documented failures remain: `Formats.SimCity2000.ReferenceCity`
  has no flat/water/sloped samples in its local fixture, and `UI.FlapLayout`
  expects mask 127 while the current result is 31. There were no new failures.
- All **37 offscreen tests passed**: the 30 authored city flyovers, route/control
  restoration, turbine lifecycle, airplane controls/propeller, cockpit frame,
  pause-menu cheat guide, drive-in playback and shoulder aim. Two tests retain
  known fixture warnings: synthetic-world drive-in attachments and two missing
  optional FLAP bitmaps in the cockpit test. Selected fresh images were reviewed
  for the close City 16 plane pass, City 18 wind farm, City 23 helicopter follow,
  cockpit wiring, shoulder pose, gear control and readable cheat guide.
- The separate **original flyover audio mixer test passed without warnings**.
  Its private Windows mixer captured active output, pause/resume, master volume,
  skip, natural completion and level teardown while silencing hardware output.
- The installed executable and PDB match the current Shipping build by SHA-256;
  all 30 installed original flyover WAVs match the source data and NonUFS
  manifest, and both installed movies match the development/publication assets.
  No binary replacement or recook was needed during this publication.

The headless warnings include existing synthetic-world/material/missing-asset
fixtures and unrelated editor-plugin HTTP connectivity timeouts. Builds retain
the warning that MSVC 14.51 is newer than preferred 14.50. No interactive gameplay
or human listening check is claimed. Original media remains locally supplied and
ignored. The release archives on GitHub retain their tagged contents; this run
publishes the combined source and does not create a new downloadable archive.

See [actual changes](ForkChanges.md#october-10-scheduled-combined-publication) and
[portable evidence](validation/PublicationIntegration20261010Results.json).
Fresh local reports and logs are under `Docs/scratchpad/publication-20261010/`;
the graphics fixtures are in their respective task scratch directories.
Individual task results below and in other validation JSON files are earlier
results, retained as history rather than presented as fresh checks.

## Aircraft, collisions, and Apache destruction - October 10, 2026

Editor and Shipping builds succeeded. All 123 selected headless tests passed
(6 warning-only fixtures). The new tests fire both weapon types at cars and
pedestrians, verify survivor-preserving Radioactivity injuries, dispatch a real
player-contact reaction, exercise climb/throttle/gear and old-save migration, and
destroy every one of City 1's 286 buildings, including six essential-service
buildings. One building is hit through the actual missile trace; the remaining
instance/footprint paths are exhausted and all 286 demolition origins are saved.

The city-intro regression checks that each plane owns independent propeller parts,
retains its translucent blur disc, animates during paused flyovers, retracts gear,
and cleans up without changing saved ambient traffic. The landing-gear plate was
rendered offscreen in both UP and DOWN positions and visually reviewed.

Warning-only fixtures retain existing synthetic-world attachment/collision messages,
an intentional missing-sound lookup, and an unconfigured tear-gas asset root. No
new gameplay test produced warnings. Installed executable and symbols were backed
up and SHA-256 verified. No assets required recooking; saves, cities, radio tracks,
and videos were not modified. No interactive gameplay or listening test was run.
Source and documentation were synchronized into the publication checkout without
altering its existing index or pushing ahead of the scheduled publication.

See [behavior and controls](AircraftAndDestruction.md),
[machine-readable results](validation/AircraftAndDestructionResults.json), and local
evidence under `Docs/scratchpad/aircraft-destruction/`.


## Medical incident placement follow-up - October 10, 2026

Sea Cliff's direct callout path passed with its actual buildings, but medical
conversion of an existing pedestrian bypassed that placement guard. Before the
fix, a real-city regression created 16 new medical incidents whose patients stayed
inside building bounds or on roofs. These were new calls, not restored saves.

Both incidental injuries (fire/crime) and player-caused injuries now validate the
existing person before allocating a medical record. Unsafe positions search nearby
outdoor ground, ordered by distance within each tile ring, and validate the final
ground-snapped position. The same actor is retained. Failed placement leaves the
actor and mission count unchanged; failed record allocation restores its old
position. Medical markers use the resulting tile. Injury initialization clears old
roof confinement and movement anchors so subsequent behavior cannot return the
patient to the inaccessible location.

Editor and Shipping builds succeeded. All 129 selected headless tests passed:
115 clean and 14 with warnings. Three existing fixtures account for collision/deck
warnings; eleven map tests captured the editor plugin's unrelated HTTP connectivity
timeout. The 45-map regression checked 51,799 building bounds, 51,633 scheduled
patient spawns and 720 incident patients, including ground/roof injuries, subsequent
behavior, marker coordinates and refusal when no safe ground exists. All incident
patients remained outside building bounds with the 32 cm clearance. Waterfront
pieces and geometry beyond the map boundary are excluded from the four on-land
injury fixtures; water refusal is tested separately. The hospital admission test
now raises an already-created patient to exercise its unchanged height guard.

Installed the Shipping executable and symbols with verified SHA-256 hashes and
rollback copies of binaries/manifests. The local publication checkout has the same
source changes; unrelated work was preserved with target-hash guards and backups.
No interactive playtest or migration of already-saved patients is claimed.
Local evidence: `Docs/scratchpad/sea-cliff-patients/`; portable results:
[SeaCliffPatientResults.json](validation/SeaCliffPatientResults.json).

## Outdoor patients and parked aircraft traffic - October 9, 2026

New state-6 medical patients reject occupied building tiles, standing building
footprints at every height, roof overhangs and water. Placement checks the final
snapped position and refuses an unsafe fallback. Parked aircraft block approaching
road vehicles by their actual bounds, including unoccupied aircraft and responders
outside the ambient vehicle pool. Both traffic AI modes hold cars stationary until
the aircraft clears the lane; dispatch speed overrides and overlap separation
cannot push a held car into it.

Editor and Shipping builds succeeded. All 84 selected headless tests passed: 81
clean and three with fixture warnings (`Missions.PlaneDeckRescue`,
`Missions.SafePassengerLanding`, `Traffic.TunnelTransit`). Both new safety tests
passed cleanly. Restoring the original production spawn functions and removing
the aircraft check made both safety tests fail, reproducing patients under roofs,
patients inside occupied tiles, forced unsafe placement, and moving traffic at
unoccupied helicopters. The fixed paths were restored and rebuilt before the final
84-test run and installation.

Installed the Shipping executable and symbols with SHA-256 verification and backups
of both binaries and manifests. Source changes are also in the publication checkout.
Local evidence: `Docs/scratchpad/patient-traffic-safety/`; portable results:
[PatientTrafficSafetyResults.json](validation/PatientTrafficSafetyResults.json).
No interactive gameplay check was run. Existing saved patients are not relocated;
intentional rooftop rescue missions and hospital handoffs retain their behavior.

## Wind speed indicator removal - October 9, 2026

Removed the compass-adjacent wind readout, its background panel and its unused
scene interface include. Editor and Shipping builds succeeded. Both existing
dashboard checks (`SimCopter.UI.SegmentedBar` and `SimCopter.UI.WaterGauge`) passed.
The compass and wind behavior are unchanged. Installed the Shipping executable
and symbols with verified SHA-256 hashes and backups of both binaries and their
manifests. No in-game visual check was run. Local build logs, test results and
installation receipt: `Docs/scratchpad/remove-wind-indicator/`.

## Whole-city intro tours - October 9, 2026

Each loaded city now gets a continuous 24-second whole-map orbit, following the
composition of the original career previews. Two visible airliners move on a
presentation clock while gameplay timers and saved fleet state remain protected.
Editor and Shipping builds succeeded. All 105 selected tests passed with the
offscreen D3D12 renderer (100 clean, five with synthetic-fixture warning groups).
Eight fixed-daylight views of Sea Cliff and Metropolis were inspected; isolated
aircraft normal-buffer captures confirm movement. The original accident tests
and gameplay regression checks also passed. No manual gameplay or final HUD/sky
inspection was performed. See [feature details](CityIntrosAndAccidents.md) and
[machine-readable results](validation/CityTourRevisionResults.json).

## Localized drive-in audio v1.0.5 - October 9, 2026

Every theater now receives the shared movie soundtrack through its own positional
speaker. Editor and Shipping builds succeeded; all six targeted tests passed.
A private Windows mixer measured HSI at all eight theaters in Metropolis before
and after switching movies, plus distance falloff, returning, pause/resume and
independent demolition. Captured output was silenced before hardware playback.
The original movie has a silent AAC track and is checked for soundtrack leakage.
Existing screen rendering/framing, all-map placement and minimap tests also passed.
No foreground flight or physical-speaker listening test was performed.
[Details](DriveInLocalizedAudio.md) and [results](validation/DriveInLocalizedAudioResults.json).

## Drive-in picture sizing v1.0.4 - October 9, 2026

Movies now stretch to full screen width and 75% height, centered with no cropping.
Editor and Shipping builds succeeded; all five targeted tests passed, including
real movie-cheat execution, rendered theater pictures, geometry dimensions,
complete source UV coverage, all-map placement and minimap markers. The frames
were inspected offscreen; no manual flight or listening test was performed.
[Details](DriveInPictureSizing.md) and [results](validation/DriveInPictureSizingResults.json).

## Drive-in visibility patch v1.0.3 - October 9, 2026

Fixed the reversed movie-screen offset that let audio play while the theater geometry
and black backing hid the picture. Editor and Shipping builds succeeded; all five
targeted tests passed, including actual lowercase `hsi` and original movie cheat
execution followed by scene captures of the real Sea Cliff theater. Both movie
pictures were inspected. No manual flight or listening test was performed.
[Before/after evidence](DriveInVideoVisibility.md) and
[machine-readable results](validation/DriveInVideoVisibilityResults.json).

The older full-suite results below remain historical; its two known failures were
not included in this focused rendering/placement/minimap/command run.

## Drive-in/radio Windows release - October 9, 2026

The v1.0.2 Windows package includes the latest Shipping executable and the existing
validated cooked content. Editor and Shipping builds succeeded. The full headless
suite ran 320 tests: 318 passed (9 with warnings), with the same two failures listed
below and none unrun. Offscreen movie playback on the newly placed Sea Cliff theater
passed. All 45 bundled city maps have drive-ins; level foundations, minimap symbols,
and radio preference persistence are covered by automation.

The release staging checks all 872 NonUFS runtime dependencies and hashes every
copied runtime file, including the 45 maps, 21 KINV tracks and both drive-in movies.
The Shipping executable's SHA-256 is recorded in
[DriveInRadioResults.json](validation/DriveInRadioResults.json).
Personal saves/settings and debug symbols are excluded. No foreground gameplay or
listening check was performed. See [release notes](Releases/v1.0.2.md) and
[implementation details](DriveInPlacementAndRadio.md).

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

## Fire alerts and victory band — October 10, 2026

- Editor and Shipping builds succeeded through `RebuildUnrealCpp.bat`.
- 70 selected tests passed: 68 clean, two with existing fixture warnings
  (`PlaneDeckRescue`, `SafePassengerLanding`); zero failed or skipped.
- Reversing the fire-report cap, player-centered band placement and distant path
  connection caused all three new regressions to fail. Restored code passed.
- Installed executable/PDB and manifest updates have verified backups and SHA-256
  receipts under `Docs/scratchpad/fire-band/installation.json`. No assets or saves
  were changed. The local publication source matches all 11 edited source files;
  no commit/push was performed in this task.
- No interactive gameplay/listening check. Remaining manual check: trigger a fire
  burst, return to the airport on different sides and walk around a terminal or
  parked aircraft while the band plays, including after loading a completed level.

See [behavior details](NpcMedicalAndHelicopterUpdate.md#fire-announcements-and-victory-band-follow-up--october-10-2026)
and [compact validation results](validation/FireBandResults.json). Full logs and
before/fixed-source comparisons are in `Docs/scratchpad/fire-band/`.

## Victory-band roster, clustering and timing — October 10, 2026

- Editor and Shipping builds succeeded through `RebuildUnrealCpp.bat`.
- All 70 selected tests passed (68 clean, two with the existing `PlaneDeckRescue`
  and `SafePassengerLanding` fixture warnings).
- Tests cover original roster totals for all 30 cities, restoration from eight to
  five and then 57 people with one leader, repeated initialization, ten-second
  shape changes, reserved spacing for large bands, following a moving player,
  actual marching/arrival, local yielding, personal-space route rejection and
  escape, walls, parked/overhead aircraft, stale paths and medical cancellation.
- Installed executable/PDB and manifest updates have verified backups and SHA-256
  receipts. All seven edited source files match the local publication checkout;
  unrelated changes were preserved. No commit/push or interactive playtest.

See [behavior notes](NpcMedicalAndHelicopterUpdate.md#loose-victory-band-and-original-roster--october-10-2026)
and [validation results](validation/BandRevampResults.json). Local evidence:
`Docs/scratchpad/band-revamp/`.
