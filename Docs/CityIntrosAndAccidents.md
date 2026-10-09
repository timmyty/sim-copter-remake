# City tours and car accidents

Each city load starts a skippable camera tour after the city, airport placement,
and any saved game have finished restoring. The tour shows the city overview,
its tallest landmark when available, waterfront or emergency services, and the
home airport. Each shot lasts five seconds, with fades between shots. Any key,
controller button or mouse click skips after the initial input guard. The world
is paused during the tour; finishing restores the currently possessed pawn,
HUD and gameplay input. Switching applications also stops the tour clock.

The route comes from the loaded map, so it also supports user-imported cities
and has fallback shots for cities without buildings or coastlines. No movie
assets or level-by-level editor setup are needed. Headless worlds skip it.

The original `FUN_0044ce50` plays a selected city's movie against `cityride.bmp`.
This implementation recreates the showcase with the remake's actual scenery;
it does not claim to reproduce the original movie's authored shots.

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
implicit occupants, saved fuse/escape, extinguishing, city route choices and
camera/input cleanup. Visual playback and live control feel require an
on-screen check; a headless pass does not establish those.

Validated 2026-10-09 with `RebuildUnrealCpp.bat` for Editor and Shipping.
The final headless run passed all 103 tests: 99 clean, four with synthetic-world
attachment/cleanup warnings, zero failures or skipped tests. Both new tests
passed without warnings. The regression run also covers missions, traffic,
dispatch, saves, career transitions, replay, knockdown, water and winch behavior.
Exact report: `Docs/scratchpad/city-intros-accidents/tests-final/index.json`.
Build hashes and the installation/backup receipt are retained alongside it.
