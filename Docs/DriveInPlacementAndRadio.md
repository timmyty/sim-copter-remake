# Drive-in placement and remembered radio station

October 9, 2026.

Cities without an authored drive-in receive the existing CO182 theater during
loading. The placement is deterministic and shared by the city renderer and the
traffic/people grid. Original SC2 files are not rewritten.

The search checks a 3x3 footprint and surrounding terrain vertices. It rejects
water, buildings, roads, utilities, large parks, airport and other protected
zones, labeled tiles and steep ground. Naturally level lots are preferred, then
the least grading and the fewest small park/tree tiles to clear. Road proximity
also contributes to the score. Dense maps such as Sea Cliff need limited grading:
the selected footprint becomes completely flat, with at most one original
altitude step of adjustment per vertex. Shared vertices under neighboring roads,
buildings, parks, utilities and water remain fixed; clear shoulders interpolate
to their unchanged outer vertices. Buried network data is preserved. Rendering
and traffic receive identical modified height/occupancy data. Existing theaters
are retained. A custom map with no suitable site is left intact and reports the
missing site in its log.

The minimap shows each standing theater with a small screen/play logo, at every
zoom level, independently of the mission and emergency-vehicle marker toggles.
Demolished theaters disappear from the overlay. Existing movie commands and
spatial audio use the added theaters through the original building registry.

As of October 10, Rock (KROC) is the default when a profile has no saved station. Every tuning path saves
the selected call sign through the profile settings, and the next city or session
restores it. Legacy saved station indices remain supported; new selections use
call signs so station-list reordering does not change the choice. If that station
is unavailable, KROC is preferred, then KMIX, then the first available station.
Each level powers the radio on, retaining a nonzero saved volume or restoring an
audible default from the zero-volume OFF setting. KINV track progress remains
independent of tuning. See [the October 10 update](CockpitAndSupportSafety.md).

Validation evidence is recorded in `Docs/scratchpad/drive-in-radio/`.

## Validation

- Editor and Windows Shipping builds succeeded using `RebuildUnrealCpp.bat`.
- All **45 bundled maps** have complete theaters. Placement is deterministic;
  every added theater has a level foundation and the correct original footprint.
- **318 of 320 headless tests passed** (309 without warnings, 9 with fixture
  warnings). The only failures are the existing `Formats.SimCity2000.ReferenceCity`
  sample expectations and `UI.FlapLayout` equipment-mask expectation.
- The offscreen `DriveIn.Playback` test passed with existing foliage-attachment
  fixture warnings: loads Sea Cliff, builds its new theater, finds the minimap
  marker, decodes both movies with audio tracks, checks looping/switching/stopping,
  then demolishes the theater and verifies removal of its marker and movie surface.
- The original validation covered KINV as the then-current fresh default (superseded
  by Rock on October 10), legacy station indices,
  persisted call signs, reordered/missing stations and unchanged volume. Existing
  station-switch, mute, track-progress and sound-dialog regressions also pass.
- The minimap logo was inspected from real palette raster outputs at all zoom
  levels; there was no foreground gameplay or live listening check.

Machine-readable results: [DriveInRadioResults.json](validation/DriveInRadioResults.json).
Builds, full test reports and raster previews are in the local scratchpad above.
No asset recook or Inspector setup is required; the existing theater and media
assets are reused. The publication checkout matches the validated development source.
