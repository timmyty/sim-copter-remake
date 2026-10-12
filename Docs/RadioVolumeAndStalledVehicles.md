# Radio volume and stalled-vehicle follow-up

## Mission priority and police-roof drops — October 11, 2026

This follow-up supersedes the earlier equipped stalled-car rate below. Ambient
breakdowns now attempt once every ten minutes with a cage, or fifteen minutes
without one. Only one unrecovered stalled vehicle is allowed, and an attempt is
deferred while original mission work is active. Player-caused accidents still
create their recovery records immediately; their existing reward suppression remains.
The saved countdown retains its 150-second units, so old saves require no migration.

Stalled-car and hull-recovery records no longer consume the original scheduler's
concurrency allowance or reset its dispatch countdown. They remain real, selectable,
saveable missions with the same completion rewards. The original seven mission
families, city weights and difficulty-dependent rescue mix remain; the previously
requested cage crime-weight adjustment also remains. In particular, the original
tier-one rescue roll is rooftop-only; boat SOS rolls begin at tier two and still
require suitable open water. This follows `FUN_004a6e60` in `.ghidra-exports`.

Automatic support prefers original missions to optional recovery when empty. It
finishes delivering aboard passengers or cargo first and respects manual assignments.

Police-roof car delivery now checks the station's actual footprint rather than a
three-metre circle around its center. Like junkyard delivery, OPEN accepts a slow
short drop with most of the rotated car footprint over the destination and its body
bottom no more than three metres above it. The sealed-cage and two-metres-per-second
speed limits remain. Successful delivery resolves the stalled-car or burglar mission
and awards it once. Wrong buildings, below-roof cargo, distant drops and excessive
height/speed are rejected.

The SOS boat's recovery job previously started while survivors were still aboard,
bypassing its rescue timer and retaining the sole capsized-boat slot indefinitely.
Recovery now starts after the riders are rescued. A new SOS can replace an empty,
unclaimed hull after its rescue mission ends, retiring the optional recovery without
reward or penalty. Occupied boats and attached cargo are protected. Failed open-water
placement preserves the existing hull; old saves with premature recovery records are
corrected during the boat tick. No save-format change is needed.

Evidence, pre-fix source snapshots, builds and automation reports are under
`Docs/scratchpad/mission-priority/`. Machine-readable final results are in
`Docs/validation/MissionPriorityResults.json`. Interactive gameplay is not claimed.

Both Editor and Shipping wrapper builds succeeded. All 56 focused automation tests
passed, with existing warnings in the PlaneDeckRescue and SafePassengerLanding
fixtures. The pre-fix run reproduced sixteen failing assertions across the scheduler
and cage/boat regression cases. Coverage includes police completion/reward
idempotency, recovery saves, helper priority/manual orders, SOS timing, occupied and
attached boat protection, and failed water placement preserving the prior hull.
The validated Shipping executable is installed in the local v1.0.1 game folder;
its previous executable is backed up and both hashes are verified. Cooked assets
and saves were not modified. No publication checkout sync or push was performed.

## Earlier radio and breakdown update

October 5, 2026. Implements the user's requested changes to sound settings and
stalled-vehicle missions.

## Behavior and causes

- Sound settings now save on OK, Back, Escape, and controller B. Back replaces
  Cancel, which previously restored the entry snapshot. Reopening the panel or
  reloading the profile restores volume, including mute and full gain.
- Both the settings store and radio subsystem preserve volume when tuning;
  each previously reset it to maximum.
- Mission plates measure the label and distance before positioning, including
  12 pixels of horizontal padding. The fixed 110-pixel plate was too narrow for
  `STALLED VEHICLE` plus distance. The measured width also drives viewport
  clamping and avoidance of other markers and UI.
- Ambient stalled cars use a 15-minute interval without a tow clamp, with no
  new ambient stall while one towable car remains. With towing available, the
  existing 150-second interval and two-car cap apply. Fresh worlds use a full
  first interval instead of the former 35-second initial delay.
- Towing capability follows the player helicopter, or the pilot's parked
  helicopter while on foot. Buying/selling the clamp changes the rate immediately.
  Existing saved countdowns retain their progress without a save-format change.
  Player-caused damage still creates its recovery job immediately.

## Validation

- New sound-dialog and runtime radio regressions failed against the original
  behavior before the fix.
- Editor and Windows Shipping builds passed using `RebuildUnrealCpp.bat`.
- **75 tests passed, 0 failed, 0 skipped** across Settings, Radio, Sound,
  Missions, and AirOperations. Two existing synthetic city tests emitted foliage
  attachment warnings (`PlaneDeckRescue` and `SafePassengerLanding`).
- Real Slate slider tests cover station changes; Escape, Enter, and controller B;
  INI disk contents; config reload; and reopening at 35%, mute, and full scale.
  Runtime radio tests verify gain and mute through tuning and resumed playback.
- Marker tests compare plates against real Slate text desired sizes and verify
  both viewport edges. Equipment tests cover clamp purchase/sale, the parked
  helicopter, timer intervals, and the unequipped one-car cap.
- No live game was launched or controlled, per `AGENTS.md` section 7. Manual
  check: adjust volume, back out, reopen, and tune another station; inspect the
  full stalled-vehicle label at screen edges. Listening and on-screen appearance
  remain a human check.

Evidence and source snapshot/diff: `Docs/scratchpad/radio-volume-persistence/`.
The installer replaces only the Shipping EXE/PDB and refreshes manifest
timestamps, with backup and SHA-256 verification. No asset recook is required.

Installed in `C:\SimCopterRemake\SimCopter Remake v1.0.1\Windows` after the user
closed the game. Both binaries match the validated release hashes. The previous
binaries and manifests are backed up in
`Docs/scratchpad/radio-volume-persistence/before-install-20261005-161003/`.
See `installation.json` for the hashes and installation record. The initial
sandboxed attempt was denied and left the original executable intact; the
authorized elevated installer completed successfully. Saves and radio tracks
were outside the installer's replacement list.
