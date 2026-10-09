# Witness photo exposure and framing

October 8, 2026.

Witness pictures were solid white instead of showing the robber. The report already
referenced the correct mission person; the failure was in the scene camera.

`USceneCaptureComponent2D::GetViewState` allocates its rendering state only when
`bCaptureEveryFrame` or `bAlwaysPersistRenderingState` is enabled. The witness camera
disabled both, leaving its final-color capture without the state needed for auto
exposure under the city's physically scaled 120,000-lux sun.

The fix preserves rendering state while retaining one-shot capture. Each new report
sets `bCameraCutThisFrame`, so exposure meters the new view immediately instead of
adapting from the preceding photograph. Camera position now uses the visible suspect
mesh's bounds and the image aspect ratio. The former fixed 320 cm distance made the
roughly 44 cm figure too small to identify. The existing obstruction search, report
queue, captions, audio, and display duration remain in use.

The initial transparency hypothesis was ruled out in the daylight reproduction:
the captured pixels had alpha 255. No alpha or gamma workaround was retained.

## Validation

- Editor Development and Windows Shipping builds succeeded through
  `RebuildUnrealCpp.bat`. Existing Unreal API deprecation warnings remain.
- `SimCopter.Witness.PhotoRendering` passed with D3D12 offscreen rendering. It
  builds the SHADES figure, reports that actual actor, checks its caption and
  one-shot behavior, and renders the production Slate photo widget.
- The controlled noon scene has **0 of 196,608 pixels clipped to white** with
  the fix. Turning off persistent rendering state in the same test reproduces
  the defect: **195,698 pixels (99.5%) clip to white**.
- All **51 mission/passenger tests passed**, including two passes with existing
  synthetic-world foliage-attachment warnings. No failed or unrun tests.
- Captures were visually inspected. This is a controlled offscreen fixture;
  no interactive city gameplay or audio check was performed.

Evidence, build logs, before/after PNGs, source backup, and reports are in
`Docs/scratchpad/witness-photo/`. The rendering regression requires an actual RHI;
it is excluded from NullRHI runs.

## Installed update

Updated `C:\SimCopterRemake\SimCopter Remake v1.0.1\Windows` with the validated
Shipping executable and PDB, and refreshed their manifest timestamps. Both files
were verified with SHA-256. No content asset or recook was required; saves, cities,
settings, and radio tracks were outside the replacement list.

The prior binaries and manifests were hash-verified in
`Docs/scratchpad/witness-photo/before-install-20261008-114951/`.
`installation.json` records the installed hashes and backup location.

Manual follow-up: trigger a new robber report in a city, check that the actual
suspect fills the photograph and is visible in daylight, and confirm the picture
stays still while the robber moves. Repeat after a time-of-day change.
