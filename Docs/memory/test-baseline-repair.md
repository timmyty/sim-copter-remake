# Reference-city and flap test repairs

October 13, 2026. These changes repair two test assumptions that had left the
automation suite at 508/510 in v1.0.11. They do not change gameplay or shipped
assets, and neither existing test is removed or disabled.

## Reference city

`SimCopter.Formats.SimCity2000.ReferenceCity` looked for exact ALTM values
`0x0004`, `0x0082` and `0x1408` in whichever reference map was present. The local
career `city0.sc2` does not contain those samples. The reader successfully loaded
the map; all three sample-presence assertions failed before the repair.

The existing test still loads a real city and checks the tile/chunk counts, name
and required chunks. It now resolves the file through the shared original-data
search roots. Its existing optional-file warning remains, but exact decoding no
longer depends on an optional commercial fixture.

The new `SimCopter.Formats.SimCity2000.AltitudeDecoding` test creates a complete
seven-map-chunk SC2 in memory and passes it through `LoadCityFromBytes`. It always
runs, even without original game files. Eight explicit samples retain the old
altitude/slope checks and also cover big-endian decoding, field maxima, the unused
high bit, row boundaries and the final map tile. Paired dry/water samples check
that XTER, rather than ALTM bit 7, determines water. The expected values follow
`FUN_004abc20` as recorded in `Docs/ReverseEngineering.md`.

## Equipment panels

`SimCopter.UI.FlapLayout` assumed the four original flaps cover every current
career equipment bit. They correctly cover `0x1f` (31); the current equipment
mask is `0x5f` (95), with capture cage bit `0x40` on its own panel. The older
documentation's mask 127 predates retirement of the tow-clamp bit `0x20`.

The repaired assertion retains the exact original mask, checks the cage's
separate/disjoint bit and checks the combined mask against all current career
equipment. Existing page, button, gating and geometry assertions remain. The
existing `SimCopter.CaptureCage.ControlsMountsAndMissionWeights` test exercises
the cage panel's purchase/sale visibility and controls.

## Validation

The pre-fix focused run reproduced both failures (one RLE test passed). The
Editor rebuild succeeded. All five focused cases passed with no warnings:
RLE, ReferenceCity, AltitudeDecoding, FlapLayout and the capture-cage controls test.

The complete suite then passed **511/511** in 287 seconds: 484 clean passes and
27 passes with warnings, zero failures, zero unrun cases. All five focused cases
also passed without warnings in that run. The added independent fixture accounts
for the increase from 510 to 511 tests. A comparison of the 458-file v1.0.11
source/configuration manifest found only the two intended test-file changes.

Full-suite evidence and source/log hashes are recorded in
[`AutomationBaselineRepairResults.json`](../validation/AutomationBaselineRepairResults.json).
Local logs and original test-file backups are under
`Docs/scratchpad/test-baseline-repair/`. Earlier release reports remain unchanged
as historical results. No interactive gameplay check or new release is part of
this test-only repair.
