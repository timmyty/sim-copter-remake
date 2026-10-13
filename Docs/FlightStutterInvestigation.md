# Progressive flight stutter — October 12, 2026

The latest diagnostic flight became increasingly choppy after approximately five minutes, after picking up a police officer with the cage and flying around. The complete trace confirms severe late CPU stalls. A responder pathfinding defect was reproduced and corrected; its contribution to this exact flight is strongly suspected, but the old trace does not contain function-level navigation markers to prove that every stall came from it.

## Recorded flight

The original capture is `Witness-Performance_20261012_215028.utrace` under `%LOCALAPPDATA%/SimCopterRemake/Saved/Profiling`. A preserved copy, log, Unreal Insights exports and analysis scripts are in `Docs/scratchpad/flight-stutter-2026-10-12/`. This is the October 12, 9:50–9:57 PM CDT Development run with combined performance tracing, DRED and Aftermath. Waterton (`city17.sc2`) loaded at 9:51:48 PM. The game exited normally through Alt-F4.

| Trace interval after startup | Mean engine frame | 95th percentile | Frames over 100 ms |
| --- | ---: | ---: | ---: |
| 180–210 seconds | 15.13 ms | 20.48 ms | 0 |
| 390–420 seconds | 21.39 ms | 36.24 ms | 58 |
| 420 seconds to exit | 29.18 ms | 133.45 ms | 76 |

These are measured `FEngineLoop::Tick` durations, not externally measured display FPS. Paused/menu intervals are present elsewhere in the trace and are not treated as flight baselines.

Individual `SimCopterGroundAgent` ticks reach about 137 ms late in the run, often repeating around 100–135 ms. Ground-agent work per frame rises substantially while agent tick counts stay near 217–219 per frame in the 180–240 and 390–440 second comparison windows. Helicopter ticks remain below 3 ms each in those windows. This directs the investigation toward expensive NPC updates rather than a growing count of active actors. A new medevac and three medical crew spawns at 9:56:59 PM coincide approximately with the onset of the worst stalls; the police pickup itself has no exact log marker.

The trace does not identify individual ground actors or internal navigation calls. The cage's procedural mesh recreation was inspected but not changed: helicopter timing does not explain these large CPU stalls. No memory-leak conclusion can be drawn from this capture.

Insights reported pre-existing memory-tag decoding diagnostics and GPU timestamp corrections up to 0.124 ms. The CPU frame and actor timings decoded. Its timer-statistics export includes GPU rows even with a CPU-thread filter; use the CPU timer names and raw thread-labelled events when interpreting the exported tables.

## Reproduced defect and correction

`GetMedicWaypoint` previously cleared `bMedicPathAttempted` whenever a target moved more than 60 cm or selection changed. This bypassed the intended one-second retry interval. A moving helicopter, or responders cycling between unreachable patients, could therefore request the complete collision-heavy 1024-node search on successive behavior updates. Longer frames also allow several behavior catch-up steps.

The same blocked-aircraft fixture measured **12 searches in 0.8 simulated seconds, 879.486 ms total, 74.991 ms worst query** before the fix. The final version starts **one** search over that interval, with **13.826 ms total and 1.125 ms worst query**. This is a controlled headless navigation benchmark, not a before/after FPS claim for the user's flight.

The correction keeps the existing grid, node limit, collision checks and route smoothing, but retains search state between updates. A responder advances at most 32 work units per update, checking a roughly 1 ms time budget between units. A physics query or one node expansion can overrun that soft budget. Catch-up calls at the same world time do not advance again. Changing targets respects the retry deadline; moving targets do not continuously restart an unfinished search.

Responders wait for complete paths, preserve patient reservations while waiting, and recheck the next segment against the current world before moving. The synchronous band caller uses the same search algorithm to completion. No save format, flight physics, cage mechanics or rendering setting changed. Two narrow CPU trace markers, `SimCopterResponderNavigation` and `SimCopterResponderPathSearch`, provide attribution in a follow-up capture.

## Validation

- Editor, Shipping and Development builds are checked before installation using `RebuildUnrealCpp.bat`.
- All 14 focused automated cases passed: moving-target budgets, sliced-route equivalence, dropped medics, steep-hill return, patient markers and reservations, police recovery and boarding, band navigation and spacing, cage controls/recovery, and rooftop exit/containment.
- The final regression also ran six simulated minutes against a continuously moving unreachable target. Mean query time stayed between **0.936 and 0.991 ms** across the six minutes; the largest individual query was **2.650 ms**. Searches completed and retried, and no incomplete or wall-crossing route was returned. This test ran headlessly with an accelerated simulation clock, not an interactive six-minute flight.
- Early validation caught a reservation regression during retry waits; it was corrected and the unchanged police recovery tests then passed. The hill fixture now waits for bounded search completion instead of expecting synchronous completion. The original benchmark's two additional route-fixture failures were corrected by using a reachable replacement target and explicitly disabling the removed wall's collision.
- Installation checks build results, source hashes, test reports, absence of a running target game, backups and copied binary hashes. See `Docs/scratchpad/flight-stutter-2026-10-12/installation.json` and `Docs/validation/FlightStutterResults.json` for the completed receipt.

## Remaining flight check

Repeat the Waterton cage pickup and several minutes of flying with `Launch-With-Performance-Diagnostics.cmd`. The new navigation markers will show whether the corrected searches account for any remaining NPC stalls. The exact original route/save was not replayed, so complete resolution of the reported flight stutter is not yet verified in-game.
