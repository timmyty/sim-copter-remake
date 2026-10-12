# Police handoffs, airport orientation and burglar traffic — October 12, 2026

This update addresses the reported cage-to-police-roof arrest, police medical retrieval, helicopter spawn heading, and Keithly burglar traffic pileup. Both installed game executables and their symbols have been updated with hash-verified backups. [Validation receipt](validation/PolicePatientAirportTrafficResults.json).

## Station rooftop arrests

`UpdatePoliceRecovery` previously excluded posted rooftop officers. A cage release also never entered the cabin-prisoner handoff, leaving its criminal on the roof without an arrest path.

The posted officer now looks for a released, landed suspect within its own roof footprint and height band, walks to physical contact using the existing contained route, cuffs the suspect, and completes the station delivery once. A suspect still attached to the cage, still falling, or on the street below cannot trigger this handoff. The officer retains its station post. All four person-crime families are covered with actual cage capture/release, fall, approach, mission completion and reward checks.

## Police medical retrieval

An uncarried, healthy aerial officer searches for criminal work first. When there is none in its existing search area, it can reserve an available medical patient through the same claim and route mechanisms as a medic, carry that person back, and load them into the helicopter. The patient keeps their identity and medical mission; loading is not hospital delivery. The officer remains police.

The medical reservation cannot be stolen by another officer or medic and is released if new criminal work interrupts an approach. A full or unreachable cabin makes the officer wait with the patient. The last free seat goes to the patient; the officer boards afterward only if another seat exists. This is a requested addition to the remake, not a claim that retail BHAV 1053 performed medical rescues.

## Airport headings

New session placement, shop delivery, missing-owned-fleet restoration and crash returns now orient helicopters toward the airport center. The tail and rear camera corridor point away from the hangar. The shared helper derives the center from opposite pad world positions. Full aircraft footprint checks still run after applying the heading.

Tests cover all twelve pad directions and the rear corridor against the actual hangar collision, plus all nine restored models fitting separately. Saved transforms are retained, so loading an existing parked aircraft does not rotate it; the change applies when the aircraft is newly placed on a pad. Fixed-wing placement is unchanged. This is an intentional departure from original `FUN_00484790`, which cleared orientation.

## Burglar traffic

The supplied screenshot shows overlapping cars around the burglar. The running diagnostics log records event 5 spawning at `(87,44)` at 16:23:59 UTC, then repeatedly leaving/returning at `(15,79)` at 16:25–16:29. That particular cluster formed after the car reached another part of Keithly, rather than at its original spawn tile.

Three reproduced defects contributed to the failure:

1. After traffic following applied a stop, `UpdateCriminalCars` and `UpdateSpeeders` replaced the speed scale with the fleeing multiplier. The free-road speed is now established before the traffic pass; later crime states can only lower its limit. Original `FUN_004b8630` calls the shared blocked-road mover `FUN_0049be50`, so fleeing does not justify ignoring traffic braking.
2. The recent car-only deadlock escape excluded every mission-owned car, including a freely driving burglar. Because both cars must be eligible, the burglar also prevented surrounding cars from escaping an accidental overlap. Cruising/fleeing/seeking burglars now participate while genuine burglary stops, driver-outside phases, police stops, taser custody, fires, aircraft obstruction, mission jams and other scripted mission vehicles retain their protections. This remains the previously requested remake recovery mechanism, not retail phasing behavior.
3. Burglar spawning omitted the ambient-car occupancy gate and recorded the first road node without assigning its move target. The next coordinator pass could interpret that node as already reached and cut across its corner. Spawning now rejects occupied or isolated sites and arms the first route hop immediately.

Regression coverage includes real movement out of an opposing-car overlap, brake preservation for both suspect types, original free-road speed, protected stops, duplicate spawn rejection, first-hop assignment, and three stop/driver-return/departure cycles on Keithly's actual road graph beginning at `(15,79)`. The screenshot's exact full vehicle arrangement was not replayed in a live game.

## Validation and installation

Editor, Shipping and Development builds succeeded through `RebuildUnrealCpp.bat`. The final combined headless run passed **117/117** cases (113 clean, four with existing fixture warnings: city-component attachment or synthetic-world teardown). This includes the earlier police, patient and airport checks, traffic, crime, dispatch, missions, career, hangar, behavior, cage, aircraft and save regressions. NullRHI does not validate rendering.

The initial police regression failed 17 assertions before its fixes. Heading checks failed seven expected assertions before the orientation fix. The stronger mission fixture needed its burglar activation stub completed before the final run. The traffic baseline reproduced nine failing assertions across three cases; a subsequent test fixture was moved just outside overlap padding so its stationary-position assertion measured braking rather than legitimate separation. The final Keithly cycle passed without warnings.

An initial headless traffic launch could not write the user derived-data cache in the sandbox; rerunning with the required cache access reached the tests. Builds retain the existing engine deprecation and unrelated `SurfaceZ` compiler warnings.

Once no SimCopter process remained, both executables/PDBs were copied to the installed `SimCopter Remake v1.0.1/Windows` directory. The sandbox initially denied executable replacement; the scoped install succeeded through the normal escalation mechanism. All four installed hashes match the built files, and all previous versions were backed up under `Docs/scratchpad/burglar-traffic/installed-backup`. Saves, content, settings, launchers and the publication checkout were not replaced. No interactive playtest was performed.

Evidence: `Docs/scratchpad/police-roof-patient/` and `Docs/scratchpad/burglar-traffic/`. The separate request about original comic pedestrian behavior is answered in the [original behavior audit](OriginalPedestrianBehaviorAudit.md); its findings were not silently added to this update.
