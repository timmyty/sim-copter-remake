# City tours and car accidents

Each city load starts a skippable **30.06366-second** flyover after scenery,
airport placement and any saved game finish restoring. The 30 career cities use
individual camera routes adapted from the original full `CITY<n>_B.SMK` movies:
overview, descent, building/terrain passes and local landmark endings. This
supersedes the earlier shared 24-second orbit based on selection previews.

City16 keeps its original wide opening and includes the near-camera plane pass
at about 20-21 seconds. Myrtle Dam ends with its wind farm visible against the
city. Pale three-blade rotors replace the original circular turbine faces at the
user's request, and rotate during gameplay as well as the flyover. Moving
original aircraft meshes and shader presentation time run without advancing
mission timers or saved traffic.

Any key, controller button or mouse click skips after the initial input guard.
Completion and skip restore the possessed pawn, HUD, input and normal aircraft
visibility, releasing only the intro's pause.
Imported cities receive a geometry-based route of the same duration. Headless
worlds skip presentation. Switching applications pauses the flyover.

See [original movie evidence, route adaptation and validation](OriginalCityIntroRoutes.md).
These are hand-authored routes against the remake's geometry, not recovered
numeric camera tracks or playback of the old movies.

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

## Historical whole-city orbit validation (superseded)

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
