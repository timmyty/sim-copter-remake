# Empty mission-message panel after HUD restore

October 13, 2026. The small dark rectangle at the top left was the mission log's
empty border: its 18-unit screen inset and 20-by-16-unit padding match the
reported screenshot at its UI scale. `SetHudHiddenForReplay(false)` forced the
message widget visible regardless of its entries. Photo Mode, replay and the
city intro all restore through that function.

`ASimCopterMissionSystemActor` now retains the transient HUD hide state. Both
message refresh and HUD restoration show the message panel only when the HUD
is visible, the message log is enabled and the log contains entries. Populated
panels use `SelfHitTestInvisible`, preserving gameplay input. Clearing the final
message keeps the border collapsed through subsequent restores; new messages
and widget recreation respect the hidden state.

The real Slate-widget regression `SimCopter.Missions.MessageLogVisibility`
reproduced eight failed assertions before the fix, including repeated empty-HUD
restores and messages arriving while hidden. All 15 focused cases then passed
without warnings: message visibility, four mission-marker cases, Photo Mode,
city-intro route/control restoration and eight replay cases. The initial separate
test file was skipped by Unreal's cached source list, so the regression is in
the existing `SimCopterMissionMarkerLayoutTests.cpp` compilation unit.

Editor, Shipping and Development builds succeeded using `RebuildUnrealCpp.bat`.
Both installed game executables and symbols were updated under
`C:/SimCopterRemake/SimCopter Remake v1.0.1/Windows/SimCopterRemake/Binaries/Win64`,
with hash-verified backups in `Docs/scratchpad/empty-mission-panel/installed-backup`.
The initial sandboxed executable replacement was denied; the approved elevated
installer completed successfully. No interactive gameplay or pixel comparison
was performed; NullRHI checks exercised actual widget visibility. Publication
checkout and release artifacts were not changed.

[Validation and installation receipt](../validation/EmptyMissionPanelResults.json).
Source backups, logs, test reports, source manifest and installation scripts:
`Docs/scratchpad/empty-mission-panel/`.
