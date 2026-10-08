# Radio volume and stalled-vehicle follow-up

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
