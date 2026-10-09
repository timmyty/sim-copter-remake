# City tours and car accidents

Each city load starts a skippable, continuous 24-second revolution around the
whole city after scenery, airport placement and any saved game finish restoring.
The camera maintains a 38-degree downward view and a fixed distance through the
orbit, with fades only at the beginning and end. Framing includes the outside
map edges and tallest roofs, adjusts to the window's aspect ratio, and reserves
space for the city title and skip prompt. There are no intermediate landmark cuts.

Two airliners using the original PLANE1 geometry move above the city throughout
the tour. Their presentation scale gives them an approximately 21 m span so
they remain readable from the whole-map camera. These temporary, non-colliding aircraft use the presentation clock;
the normal ambient fleet's visibility and saved state are restored afterward.
A world-scoped render-time override also advances shader-driven water and clouds.
Gameplay remains paused, so the showcase cannot consume mission time or alter
saved traffic. Switching applications stops the presentation clock as well.

Any key, controller button or mouse click skips after the initial input guard.
Finishing restores the possessed pawn, HUD and gameplay input and releases only
the tour's own pause. The route comes from the loaded city, including imported
cities and maps without landmarks. No per-level setup or new movie assets are
needed. Headless worlds skip the presentation.

Original evidence: `FUN_0044ce50` selects a city movie against `cityride.bmp`;
movie constructor `FUN_00448170` uses a vtable whose open function `FUN_00448400`
calls `SmackOpen`. The bundled converted career previews
`Content/Generated/Movies/Career/CITY0_S.mp4` through `CITY29_S.mp4` are 200x108,
about 5.3 seconds long. Inspected city 0 and 29 frames show the whole square map
rotating steadily, including moving clouds/aircraft. These are career-selection
previews, not the full cityride movies. This revision follows their composition
with a slower orbit of the remake's actual scenery; it does not claim an exact
reconstruction of the unavailable full movie camera paths.

A helicopter's first moving contact with a car now tips the body 90 degrees,
stops the car and immediately starts its fire. It posts the original event
`0x34` against event id `-1`: **Non-Mission Event: You caused an accident**,
deducting 100 points and 75 cash once (both retain their zero floor). Repeated contact cannot restart the fuse,
repeat the penalty or create duplicate drivers. Actual NPC passengers exit;
ambient cars without materialized occupants produce one driver. Survivors run
away using ground and wall checks, then return to their normal behavior.

After four seconds the burning car explodes with the existing particle/debris
effects and original explosion sound, then leaves the traffic pool. Dousing it
before the explosion leaves a tipped, towable car with suppressed recovery
rewards. Accident fires are visible even when the mission-record pool is full.

Ground-agent save payload version 7 preserves the tipped state, evacuation
latch, remaining fuse and survivor escape. Versions 1–6 remain readable;
legacy fourth-hit burning cars acquire the tipped visual on load.

Original evidence: `.ghidra-exports/0049f680.json` case `0xc` ignites the car
and posts `0x34/-1`; `0049fd00.json` sets a 900-tenth-degree roll and fire
state; `0049ff00.json` emits debris, explosion sound 4 and the car-burned event.
The four-second fuse deliberately retains this fork's pacing: the original
sets `0x780000` (120 seconds). The earlier second-hit stall/fourth-hit fire
rule in `AirOperations.md` is superseded.

Validation evidence and source baseline: `Docs/scratchpad/city-intros-accidents/`.
Automation covers first impact, penalty/message deduplication, actual and
implicit occupants, saved fuse/escape, extinguishing, city framing and
camera/input cleanup. Visual playback and live control feel require an
on-screen check; a headless pass does not establish those.

Initial implementation validated 2026-10-09 with `RebuildUnrealCpp.bat` for Editor and Shipping.
The final headless run passed all 103 tests: 99 clean, four with synthetic-world
attachment/cleanup warnings, zero failures or skipped tests. Both new tests
passed without warnings. The regression run also covers missions, traffic,
dispatch, saves, career transitions, replay, knockdown, water and winch behavior.
Exact report: `Docs/scratchpad/city-intros-accidents/tests-final/index.json`.
Build hashes and the installation/backup receipt are retained alongside it.

## Whole-city tour revision validation

The revised orbit passed all 105 selected tests with the D3D12 offscreen renderer:
100 clean and five with synthetic-world attachment/cleanup warning groups, zero
failures or unrun tests. The earlier NullRHI run passed 104 tests; the additional
test requires a renderer. Framing checks cover 241 points around the orbit,
portrait through 32:9 screens, translated maps and tall skylines. Save bytes for
the normal ambient fleet remain identical before, during and after the tour.

Sea Cliff and Metropolis were rendered at four bearings with their normal
procedural surrounding terrain, a 120,000-lux sun and fixed exposure. All eight
views were inspected. The aircraft-only normal-buffer comparison confirms real
mesh movement with the camera held still (198 and 190 changed pixels respectively).
Using a component show-only list excludes water, lights and texture streaming
from this assertion. The fixture initializes material parameters, waits for asset
compilation and captures on separate engine frames. Unlit/final-color captures in
an unlit synthetic world were unsuitable for this check; those failed attempts
are preserved in the scratch evidence rather than counted as passes.

Editor and Shipping builds succeeded through the required wrapper. Source hashes,
test results and frame hashes are in [the validation record](validation/CityTourRevisionResults.json).
Logs, original-preview contact sheets, rendered frames and the installation backup
are in `Docs/scratchpad/city-tour-revision/`. No manual gameplay, final HUD/sky
composition or cloud-motion inspection was performed.
