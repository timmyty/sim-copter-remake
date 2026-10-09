# Aircraft passengers, cows, emergency coverage and planes — 2026-10-08

User-requested extensions to the original mechanics. Runtime changes apply to all
career and custom cities; no city files or original GEO files were edited.

## Cabin bodies

All nine helicopter types now split their authored canopy glazing into the existing
transparent cabin material. Occupants use the existing seated-body mesh generator,
with model-specific positions for the pilot and every passenger seat. The manifest
drives body count, clothing variation and medical bandages; bodies disappear on exit.
The Apache retains its original zero-passenger capacity. AI pilots are represented.
The new plane has a pilot and one passenger body too.

Cabin positions use authored body coordinates at model scale 0.25, with a relative
scale correction for other scales. Black tires, exhausts and propeller blades stay
opaque. Existing first-person fuselage hiding remains part of the cockpit view.

## Cow separation

`IsCow()` recognizes `Coww` and behavior class 17. Human interaction reactions,
injured/dead poses, incidental/player-caused medical missions, carried-person pickup,
carrier boarding and passenger manifests reject cows. Physical helicopter/vehicle
knockback remains active. This prevents a bumped cow entering hospital delivery.

## Roads and emergency response

- Responders travel at 1,200 cm/s. General car movement aligns velocity to the active
  road segment and caps each step at its waypoint, preventing high-speed overshoot.
- Police retargeting skips the route origin instead of doubling back to its center.
  Pursuit stop range now measures the police car's actual tile, not its destination.
- Dispatch chooses by weighted route travel time, including tunnels, bridge height
  and highway links. Its accepted road IDs match the actual road graph.
- Each city rebuild checks every road node against fire, police and ambulance
  headquarters. Travel estimates use 60% of service speed plus 0.12 s per waypoint.
  Coverage over 34 s adds a marked roadside service depot; every disconnected road
  component consequently has local service. Roads are not joined through terrain.
  Dispatch rejects a candidate route at 45 s or more and tries the next candidate.
- Existing station order is retained and added depots are deterministic. The original
  five-vehicle pool per service remains in place; busy/destroyed units still matter.

## Transport plane

The original PLANE1 GEO object (0x12e) is a playable aircraft with runtime type 9,
spawned on an available airport pad after saved/career aircraft restoration.
Existing helicopter catalog indices, nine ownership bits and prices remain unchanged.
The registry and save validation explicitly accept the plane as an additional type.

Use the existing climb control to start the engine and take off. Forward input
accelerates to 3,200 cm/s; neutral airborne cruise is 1,800 cm/s. Turning reaches
105 degrees/s in flight. Backward input brakes; descend while braking to land.
Neutral ground input stops the plane so passengers can board. Unpowered planes glide
down; water landings crash. Shared collision, fuel, damage, camera and save systems
remain in use. There is no rotor lift or hover.

Only one Transport fare can board. Medical patients, rescue passengers, police/medics,
custody passengers, harness boarding, emergency equipment, ground dispatch and air
support orders are refused. The mission scheduler, map cycling and markers expose
Transport jobs while a plane is possessed, with a fare requested when none is active.
Existing emergency deadlines pause during the sortie and resume in a helicopter.
Transport rewards use the normal mission payout path.

## Evidence

`Docs/scratchpad/aircraft-road-update/` contains source baseline/diff, builds,
automation reports, exported runtime meshes and offline Blender previews.

`SimCopter.AircraftRoadUpdate.AllCitiesAndAircraft` checks real aircraft meshes,
capacity, cow protections/launch velocity, plane controls and save restoration,
mission restrictions, and all 45 supplied cities (30 career + 15 custom).
It verifies every road node for all three services, then actually advances the
vehicle/dispatch/tunnel simulation to the worst-covered destination for each service.
All 135 response cases arrived in 11.4–17.45 s. These are headless simulations with
the real city road data, without a rendered city's collision meshes or live traffic.
The conservative graph coverage bound is at most 34 s.

Full-suite report: 300 tests, 298 passed (seven with warnings), two pre-existing
failures: `SimCopter.Formats.SimCity2000.ReferenceCity` and `SimCopter.UI.FlapLayout`.
See `Docs/ForkValidation.md` for the previously recorded failure explanations.
The final focused report (`tests-verified/index.json`) passed all 64 tests (62 clean,
two with warnings), including the last plane restrictions and the same outer-record
then runtime-state save restoration sequence used by the game. Both Editor and
Shipping builds passed using `RebuildUnrealCpp.bat`.

Offline cabin previews verify geometry placement with approximated materials; they
are not Unreal gameplay screenshots. No interactive flight, visual or sound check
was performed. No new content asset or dependency is required.

Installed the validated Shipping EXE/PDB into
`C:\SimCopterRemake\SimCopter Remake v1.0.1\Windows`, with SHA-256 verification.
The replaced files and manifests are backed up at
`Docs/scratchpad/aircraft-road-update/before-install-20261008-131607/`.
Only the executable, symbols and their manifest entries were changed; saves, city
files, content archives and the custom radio library were preserved.
See `installation.json` and `release-files.json` in the evidence directory.
