# Mission and helicopter improvements

Requested September 30, 2026; completion verification October 5, 2026.

## Changes

- Transport passengers can be delivered onto their destination building's flat roof.
  Delivery checks the destination footprint, actual collision surface, and release
  clearance. Other rooftops and airborne releases do not count. Pedestrian walking
  and falling use triangle collision; delivered roof pedestrians receive the existing
  roof containment behavior. Rescue passengers retain their separate delivery rules.
- Ditched-aircraft rescues create survivors on the aircraft's actual mesh instead
  of using the unrelated capsized-boat location. Wrecks provide walkable collision and are floated high enough to keep all sampled survivor positions above the swimming band.
- Harness riders are raised so their hands are closer to the rescue triangle.
- Arresting a criminal recalls the nearest eligible police dispatch. Its assignment
  marker disappears while the unit returns; other dispatches remain assigned.
- A dedicated belly camera follows the spotlight's target. Cycle **C** through
  Chase, Orbit, Rescue, Cockpit, and Spotlight. Existing camera zoom controls adjust
  its field of view. The mode survives saving/loading.
- The dashboard has a **WIND … KT** readout immediately left of the compass. It
  samples Unreal world wind sources, and reads calm when none exist. This change
  does not introduce a separate wind physics simulation.
- Buckets can draw from authored water polygons (pools/ponds) and rendered water
  beyond the original map bounds. Land elsewhere in the same tile remains dry.
  Supply remains available when animated-water rendering is disabled.
- Robber spawns send a one-time street-level scene photograph of the actual suspect
  to the pilot, with last-seen grid coordinates and a spoken witness report. The
  existing SHADES robber figure gets the sunglasses description. The photo panel
  lasts 18 seconds; speech waits for the original dispatcher queue. It does not
  take keyboard focus or continuously track the suspect.
- New Windows game icon: `SimCopterRemake/Build/Windows/Application.ico`, with a
  PNG master alongside it. Contains 16, 24, 32, 48, 64, 128, and 256 pixel images.

## Art and audio provenance

The built-in image-generation tool combined the supplied `simcoptericon01.ico`
title styling with the full `jetranger.ico` helicopter/city composition. Prompt:
“Create a square Windows game icon; preserve the entire helicopter and city
composition; overlay only SIM COPTER across the top using the first reference's
title styling; omit DEMO and demo badges; retain the retro pixel-art aesthetic.”
The references were converted to PNG for input; the generated master was exported
to ICO without further artistic editing.

Witness WAVs were generated offline with installed Windows SAPI speech, using
`Docs/scratchpad/mission-improvements/make-briefings.ps1`. They are packaged loose
under `Content/Briefings` through the module's runtime dependency declarations.
No online speech service or new engine plugin is required.

## Verification

Editor builds and regression results, source backups, and a review diff are kept in
`Docs/scratchpad/mission-improvements`. The original completed regression run passed
70 tests (two fixture warnings, zero failures). The completion pass adds tests for
the actual aircraft-deck rescue and spotlight camera/save behavior.

Rendering and listening still require a live in-game check: harness hand alignment,
photo exposure/composition, HUD readability, and spoken-report mix. Following
AGENTS.md section 7, validation uses builds and headless tests without taking over
the user's foreground game session. No claim of in-game visual verification is made.

### Completed validation — October 5, 2026

- Editor and Shipping wrapper builds succeeded (`editor-complete.log` and
  `shipping-build.log`). Engine headers emit deprecation warnings.
- All 72 targeted automation tests passed: 69 clean passes and three passes with
  warnings, zero failures or skipped tests. The warnings are the test world's
  existing static/movable city-component attachments and missing world context.
  Report: `Docs/scratchpad/mission-improvements/tests-complete/index.json`.
- Full Win64 Shipping cook, stage, pak/iostore, package, and archive succeeded.
  Both MainMenu and CityRender are in the cooked manifest. Both witness WAVs and
  all six original-data trees are present in the loose-file manifest.
- Package and logs: `Docs/scratchpad/mission-improvements/Package` and `package.log`.
- The installer preserves local cities and saves and verifies the copied package
  with SHA-256. Installation evidence: `Docs/scratchpad/mission-improvements/installation.json`.
  An open notes file prevented moving the old directory; installation instead
  completes its backup and copies individual files in place without closing apps.


Installed successfully in the existing Windows folder; all 858 copied files verified by SHA-256 and zero missing NonUFS manifest files. The installed bootstrap executable's extracted icon was inspected and contains the new artwork. Backup: `C:/SimCopterRemake/SimCopter Remake v1.0.1/Windows-before-mission-improvements-2026-10-05`.
