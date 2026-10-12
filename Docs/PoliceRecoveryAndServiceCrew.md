# Police recovery and checkup service crew

Requested October 11, 2026.

## Reported behavior

- A dropped officer ignored a tased arsonist instead of carrying them into the helicopter.
- A handcuffed arsonist aboard did not produce a minimap route to a police station.
- An officer dropped five metres from a disabled burglar car did not arrest its occupant.
- Paid helicopter repair, refueling and teargas refills lacked the original service crew.
- Police-station roofs should also accept cage deliveries of burglar cars and stalled vehicles.
- A floating JUNKYARD world label should appear only while the player's cage holds a stalled vehicle.

## Evidence and implementation

`StunForArrest` suspends the suspect's behavior VM. The original-style person search
requires an active VM, and `PushBehaviorReaction(1060)` also refuses suspended people.
The shipped `people.df` BHAV 1053 searches uncaught people; BHAV 1060 credits arrest
immediately and walks the suspect to a police car. Neither program implements the
remake's taser-and-helicopter custody loop.

Deployed state-7 officers now select disabled people independently of their VM and
physically approach using the existing obstacle path and contact checks. They carry
the same actor, handcuff it, return to the helicopter, and board together when two
seats and boarding clearance are available. Capturing the person does not award
delivery credit; the police-station handoff remains responsible for that.

The officer scan also finds immobilized mission burglar cars with their driver still
inside. Physical officer contact completes the existing burglar mission. Moving,
towed, wrecked, resolved, and driver-outside cars do not enter that recovery path.
This extends the original spotlight-dependent pursuit to the remake's electrical stop.

The cockpit map derives a police destination from actual handcuffed passengers and
the nearest serviceable police roof, independently of selected mission and optional
blip visibility. The route updates as the helicopter moves and disappears after
delivery. It does not assign a police destination to medical patients.

The capture cage also accepts slow, low releases over police-station roofs for burglar
cars and stalled vehicles. It uses the existing idempotent vehicle completion path.
Boats retain their harbor destination, and the junkyard remains available for cars.

The existing mission HUD now adds an amber JUNKYARD label at the nearest yard when
the player's cage contains a stalled vehicle. It uses the same icon, distance,
projection and edge-pinning layout as other world labels. Release or delivery
removes it; a stalled car elsewhere, an empty cage, a moving getaway car, or boat
cargo cannot trigger it. This condition does not change the existing minimap icons
or physical recovery rings.

## Original service crew

Direct executable evidence in `.ghidra-exports`:

- `FUN_004385c0` counts positive repair, fuel, and teargas purchases and calls
  `FUN_004c1ea0` with that count.
- `FUN_004c1ea0` makes up to twice that many placement attempts to spawn the requested
  number of class-6 workers with program `0x4b4` (1204).
- `FUN_004c1fb0` suppresses duplicate nearby building workers.
- Shipped BHAV 1204 (`Mechanic`) runs BHAV 1020 (`Work on Copter`), including approach,
  work counter, original animation/voice requests, and eventual deactivation.

Thus teargas-only service requests a mechanic too; all three paid categories request
up to three mechanics using the same behavior, rather than separate fuel-specific art.
The original spawn function has no airport-only condition. The remake retains its
existing automatic airport checkup offer and now creates crew for accepted grounded
service orders, including a manually opened checkup away from the base.

Crew use clear positions beside the aircraft, existing original figure mapping
(class 6's unavailable `BLUE` key already maps to `SUIT` in this port), and the original
behavior program. Scripted mechanics can walk across airport aprons without ignoring
physical walls or water. Their program identity survives ordinary agent saves.

Placement checks the worker's final ground position. Reusing the higher passenger
doorsill position before snapping down allowed three workers to overlap, blocking
all of them. The combined-service test exposed this and verifies separate grounded
positions as well as progress in the original work program.

## Validation

The Editor and Shipping builds passed. The focused headless report contains 18 successful results
(17 logic cases and the existing taser visual case's headless no-op), with no failed
or warning cases. Coverage includes all four suspect states, officer/custody return,
waiting for two cabin seats, nearest-station line pixels and label, disabled burglar
arrest at five metres, both police-roof vehicle deliveries, repeated-reward rejection,
the conditional world marker, all three individual paid services and their combined
crew, and related medical/taser/boarding regressions.

Builds, test reports and baseline snapshots are recorded under
`Docs/scratchpad/police-recovery/`. The installed executable was replaced after
verifying the previous hash, creating and verifying its backup, and checking the
installed copy against the new Shipping hash. No cooked assets, saves or user data
were changed. [Machine-readable results](validation/PoliceRecoveryAndServiceCrewResults.json).
No interactive game check, publication sync or push is claimed.

## Longer pursuit and suspect-first boarding — October 11, 2026

The deployed aerial officer now uses the existing thirty-tile search radius
(120 metres at the default tile scale) for both disabled people and active,
uncaught criminals. Routed pursuit uses shipped Run-10 speed 25 and continues
until physical contact, tracking moving targets. Previously BHAV 1053's wide
search fell back to random short runs and a two-tile close search. Contact with
an active criminal invokes original reaction 1060 and resumes BHAV 1053 record 26,
the original 2Gab hold. Existing caught/resolved people are excluded.

Disabled suspects retain the separate physical carry-to-helicopter flow and
station-delivery credit. One free seat now admits the suspect; the officer boards
only if another remains. With no seats, the officer retains custody and retries.
This supersedes the two-seat requirement described in the initial implementation.
Tests reproduce the former one-seat and long-pursuit failures and now cover moving
and disabled suspects starting fifty metres away, all four criminal states, full
cabin retry, and original hold after contact. Shared terrain routing also inherits
the medic hill fix.

All 35 combined headless tests and both builds passed; the playable executable is
installed with a verified backup. No interactive playtest was performed. See
[combined results](validation/MedicPoliceAndHangarResults.json).
