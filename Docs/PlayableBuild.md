# Local playable build

User request: install every change from the supplied `sim-copter-remake-main` fork
into the playable `SimCopter Remake v1.0.1`, including fewer simultaneous missions.

Source baseline: wyozi/sim-copter-remake commit
`25d81802b065eac5fee1dbd74afb75d1e59004e8`, 31 commits ahead of JamesIV4/main.
All 4,427 downloaded source files matched GitHub blob hashes before local edits.
Evidence: `Docs/scratchpad/fork-source-verification.json`, `fork-comparison.json`.

Local changes:
- Restore Base Location in the scheduler count from FUN_004a6e60. Tier 1 normally
  has one job; a second waits the full 380-second interval. Higher tiers retain
  increasing availability. Keep the fork's other gameplay and performance changes.
- Add `SimCopter.Missions.OriginalMissionAvailability` covering all four tiers.
- Rename the `Controller` local in `SimBenchView` to resolve MSVC C4458.
- Let the required build wrapper find UE 5.8 in the Epic Launcher installation.

Build inputs:
- UE 5.8.3 at `C:/Program Files/Epic Games/UE_5.8`.
- Official NVIDIA UE 5.8 bundle 8.8.0: DLSS, StreamlineCore, StreamlineDLSSG,
  StreamlineNGXCommon, StreamlineReflex in project `Plugins/NVIDIA`.
- Original data copied from the installed game's `Windows/SimCopter` into
  `Reference/SimCopterOriginalGame`.
- Loose Generated movies/loading art copied from the installed game into source
  Content/Generated. This preserves the original licensed data locally.
- Git LFS source assets restored with `Docs/scratchpad/restore_build_assets.py`,
  verifying each payload's size and SHA-256. BakeCityAtlas still needs to run
  inside the rebuilt editor to generate its locally derived assets.

Delivery plan: build and test editor; bake city atlas; run the build wrapper with
`Shipping`; cook/stage/archive with RunUAT into a fresh scratch output; verify
both maps and the original-data staging manifest; smoke-test the package; back
up the original Windows folder and install the complete new Windows package.
Preserve saves and custom cities. Do not claim readiness before packaging succeeds.

Build/test results and final installation status will be appended below.

## Validation so far

- Editor build succeeded (`playable-editor-build.log`, incremental test rebuild
  `playable-editor-rebuild.log`).
- 58 regression tests passed: missions, settings, low-power settings, and day/night
  length. One passed with a fixture cleanup warning; zero failures. Includes the
  mission-availability checks at all four difficulty tiers. Report:
  `Docs/scratchpad/playable-tests-final/index.json`.
- An existing settings test used a relative temporary INI path while SaveConfig
  normalized its cache key. Normalizing the test's path fixed its second reload;
  production settings behavior was unchanged.
- All LFS pointers under Content were replaced by verified assets. The large map
  was retrieved via GitHub media and matched its recorded SHA-256
  `eaf08e1f06838e1e5e67b6eb0a539d64ce3e35844f49b4d47610e78a6531f598`.
- City atlas: 157 generated assets, five day pages, five night pages, three painted
  window masks, 68 direct images, both terrain materials. Reload/bake verification
  completed with zero errors and warnings (`playable-atlas-verify.log`).
- Shipping build succeeded (`playable-shipping-build.log`):
  `SimCopterRemake/Binaries/Win64/SimCopterRemake-Win64-Shipping.exe`.
- Packaging succeeded with RunUAT BuildCookRun, Win64/Shipping, skipbuild, cook,
  stage, pak, iostore, package, archive, prerequisites. Fresh archive destination:
  `Docs/scratchpad/PlayablePackage`; log: `playable-package.log`.
- All NonUFS manifest files exist; both MainMenu and CityRender are in the cooked
  manifest, alongside the intro movies, loading art, original game data, and NVIDIA
  runtime libraries.
- Launched the package through its bootstrap EXE with a separate `-UserDir` test
  profile. Visually verified intro playback, main menu, career selection and Level 1
  (Sea Cliff): city, helicopter, HUD and map rendered. The first test launch ended
  around an Escape keypress (the main menu maps Escape directly to Quit); the repeat
  launch proceeded normally without skipping the intro and loaded the city.
- Installed to `C:/SimCopterRemake/SimCopter Remake v1.0.1/Windows`.
  Original folder retained as `Windows-original-2026-09-30` in that same parent.
  Original city files and any local Saved folder were preserved; normal personal
  save profiles were not touched by testing.
- Installed Shipping EXE SHA-256:
  `6027F1EB776E7E6AAAD337BF5B835726652A4577F95440DFA15FF2ECF65C5EC3`.
  Installation identity and file verification are in `Docs/scratchpad/playable-installation.json`
  and `playable-installed-verification.json`.
- The preview remains open from the scratch package with its isolated SmokeProfile;
  normal play should use the installed Windows/SimCopterRemake.exe.

## Siren mix update — 2026-09-30

- User-requested adjustment in `UpdateEmergencySirenAudio`: fire, police, and
  ambulance service sirens use -6.02 dB (approximately half gain) and half the
  previous distance range, fading over 60 metres instead of 120. The hose loop
  retains its existing volume and range.
- Editor and Shipping wrapper builds succeeded; all seven `SimCopter.Sound`
  automation tests passed without warnings. Logs/reports: `Docs/scratchpad/siren-*`.
- Updated the installed Windows package's Shipping EXE and matching PDB directly
  (C++ change only; no asset recook needed), verifying both against the build with
  SHA-256. Previous binaries are retained in
  `Docs/scratchpad/before-siren-mix-2026-09-30`.
- This supersedes the installed EXE hash above. No in-game listening check was run.

## Cow silhouette adjustment — 2026-09-30

- Reduced the Coww figure's diagonal torso/head filler thickness and length, reduced
  its exaggerated belly strokes from 4x to 2x, and slimmed rear legs and udder.
  This is a remake-only art adjustment requested to reduce the blob-like silhouette.
- Updated source Config/FigureAdjustments.json and the installed Windows package's
  loose config. All other figures preserved. No C++ or animation data changes.
- Validated JSON and finite geometry for both standing frames and all eight running
  frames; visually inspected the generated contact sheet from both sides. Preview
  uses the existing Python geometry mirror, not an Unreal in-game capture.
- Before/after: Docs/scratchpad/cow-shape-comparison.png. Full frame sheet:
  Docs/scratchpad/cow-all-frames.png. Original configs retained in
  Docs/scratchpad/before-cow-shape-2026-09-30. Restart the game to reload adjustments.

## Cow replacement model — 2026-09-30

- User explicitly authorized abandoning original cow visuals. Replaced Coww's
  stroke assembly with a distinct black-and-white procedural cow, including head,
  muzzle, eyes, ears, horns, legs, split hooves, udder, and hanging tail.
- Existing behaviour and animation timing retained; new diagonal-pair gait.
  Legacy Coww JSON adjustments are bypassed. Other figures are unchanged.
- Editor and Shipping wrapper builds succeeded. Both SimCopter.Figures tests
  passed without test warnings/errors, including all ten cow frames, outward
  triangle winding, finite geometry, grounding, and frame switching.
- Visually reviewed actual C++ mesh exports from three angles and a walking pose:
  Docs/scratchpad/cow-redesign-preview.png. Not verified in a live city.
- Installed matching Shipping EXE/PDB to the playable Windows package with
  SHA-256 verification. Previous binaries and edited source backups are in
  Docs/scratchpad/before-cow-redesign. No asset recook needed.
- Full implementation note: Docs/memory/cow-visual-redesign.md.

## Mission and helicopter improvements — 2026-10-05

- Implemented roof transport delivery/walking, visible aircraft rescue survivors,
  raised harness grip, robber witness photograph and speech, nearest-police recall,
  additional scoopable water, compass-adjacent wind speed, dedicated spotlight
  camera, and the new SIM COPTER/helicopter icon.
- Editor and Shipping builds and full packaging succeeded. All 72 targeted tests
  passed (three with fixture warnings). No live visual/audio check was performed.
- Installed to the existing Windows folder, verifying all 858 copied files by
  SHA-256; no runtime-manifest files are missing. Original cities/saves preserved.
- Prior package retained in `Windows-before-mission-improvements-2026-10-05` beside
  the installation. An open RemakeNotes.txt prevented folder renaming; completed
  the backup and installed files in place instead, preserving that open file.
- Installed Shipping EXE SHA-256:
  `DC9D25C4F746538712F2128F1AF83346CC7567D2C091A8682592DC50FD2CE760`.
- Details: [Mission improvements](MissionImprovements.md). Full verification and
  backup evidence: `Docs/scratchpad/mission-improvements/installation.json`.

## Invoke the Revoked radio — 2026-10-05

Installed KINV with all 21 songs: Dancing to Our Doom first, then screenshot
songs 1–13. Track and seek position save on station switch/power off/exit and
periodically to Saved/InvokeRadio.ini. Radio volume now covers 0–200%; disable
Auto-Quiet to reach 200%. All imported durations round to the screenshot values
(maximum difference 0.490 seconds). Editor and Shipping builds passed; 27 radio,
sound, and settings tests passed. Installed EXE/PDB and 21 WAVs with SHA-256
verification and prior-binary backup; no cooked assets changed. No live listening
or UI check. Details and source-selection caveats: [Invoke radio](InvokeRadio.md).

## Hospital entrances and responder update — 2026-10-05

- Hospital/police rooftop spawns require clear, separated positions. Both buildings
  now have signed ground responder posts; the hospital entrance accepts patients
  from a landed helicopter, carried by the pilot, or set down beside its medic.
- Hospital-only cladding, roof and antialiased helipad H materials preserve the
  original mesh/collision. Helicopter-car impacts play a new metal crunch with
  short sparks, using the existing damage cooldown.
- Editor/Shipping builds and 68 focused tests passed (two older fixture warning
  groups). Corrected SM5/SM6 material compilation, cook/package and asset inspection
  passed. All 870 NonUFS files exist. No live gameplay/listening check was run.
- Updated nine installation files with SHA-256 verification. Complete backup:
  `Windows-before-hospital-response-2026-10-05`. Preserved 45 local city/save paths
  and all 21 radio tracks.
- Details: [Hospital and responder posts](HospitalAndResponderPosts.md).
  Evidence: `Docs/scratchpad/hospital-response/installation.json`.
