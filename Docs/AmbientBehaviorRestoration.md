# Ambient behavior restoration — October 12, 2026

The requested update restores the original casualty-gawking target search, fixes NPC selection of the player's aircraft, and adds bounded civilian visits to parked helicopters using original Simlish and figure animations. The earlier police, patient, airport-heading and burglar-traffic fixes remain included.

## Original behavior restored

Ambient BHAV 600 already calls BHAV 273, “Gawk at corpse (inc. medevac)”. Its class-15 object lookup was missing. `FindNearestGawkCasualty` now implements that lookup without reserving a medical patient or awarding any mission credit. Candidates must be visible, uncarried, outside hospital tile D1, and either medical state 6 or written off. The nearest candidate is selected by 3D Manhattan distance; the existing VM applies the original strict three-tile range. The original gawk, voice, idle, scatter and departure branches continue to run from `people.df`.

Ground truth: the old `004cac70` export stops before its jump-table cases. The user's local Classics disc executable contains the equivalent function at `004d19f0`; table `004d1eb0` entry 15 points to `004d1c65`, equivalent to the old `004caee5` case. The scan checks person offsets `142` (active), `152` (visible), `1a0` (carrier), tile D1, then `15e != 0 OR 148 == 6`, and compares distances. The local executable was matched by the unique function prologue and the other table cases, rather than assuming identical addresses across versions.

The remake retains recoverable bodies after their VM stops, so those visible bodies also remain candidates. Falling people are excluded. Gawkers stop outside the pickup space and abandon a selected person once a rescuer carries them. These are explicit adjustments for the remake's physical rescue system.

## Aircraft identity

NPC helicopter queries now share `SimCopterHelicopterParking::ResolveCurrentAircraft`: possessed aircraft, the on-foot pilot's parked aircraft, legacy owner reference, then a deterministic fallback. The old nearest-aircraft query could choose a different fleet member while the pilot was walking nearby. Support, hidden and destroying aircraft are excluded. Intentionally selected fixed-wing aircraft retain existing passenger-system support.

## Requested helicopter visits

This gathering is an extension using original behavior assets; a generic retail civilian inspection caller was not established by the audit.

- At most three existing nearby ordinary Sims participate. No extra crowd is spawned.
- Visitors require a parked, stationary helicopter and reachable supported ground. They use a group of spaced locations beyond its rotor footprint, preferring the front and using a flank when a building blocks the front. Both passenger-door approaches and the tail remain clear.
- They physically walk over, acknowledge the aircraft, and use `2Gab`/`HipH` with the original `004c6970` conversation voice-event mapping: 10, 1, 4, 11, 2, 18, 6, 7, 3. Replies can face another visitor; speech attempts are staggered and limited to two per visit.
- A visit lasts eight seconds after arrival; approaches time out after fourteen seconds. Each participant then has a 25–45 second cooldown. A reachable outward departure is ground-checked.
- The pilot and responders take priority when approaching a visitor. Aircraft motion or switching, nearby casualties/fires, original reactions, injury, carrying and traffic avoidance cancel curiosity. Mission people, criminals, medical staff, animals, mechanics, statue scripts and band members keep their own behavior.
- The original ambient stack is paused, not replaced. Reservations are transient and released when restoring a save; no save-format change is required.

## Validation and evidence

Editor build and 127 combined headless cases passed: 123 clean, four with existing fixture warnings (`Missions.PlaneDeckRescue`, `Missions.SafePassengerLanding`, `Traffic.TunnelTransit`, `UI.HangarPurchaseParking`). The new tests exercise the actual class-15 query and shipped BHAV, active/parked/owner aircraft selection, crowd capacity, physical approach, door spacing, conversation progression, timed departure, pilot/medic yielding, casualty priority, reaction interruption, aircraft motion, blocked-front flank selection and save/load cleanup.

Initial tests demonstrated the missing class-15 lookup. The first multi-aircraft fixture required explicit controller registration before it could test the on-foot player correctly; its corrected version passes with the shared resolver. Restart fixtures also needed a fresh behavior context because the original `ResetToState` intentionally retains reaction attributes. These fixture problems are not recorded as game defects. Unreal's cached source list initially omitted the new test file; touching the unchanged module build-rules file forced discovery.

Scratch evidence is in `Docs/scratchpad/ambient-curiosity`: the original disassembly, before-change source snapshots, review diff, source manifest, build logs and exported test reports. Original executable SHA-256: `a20bc20616f705df9d2a700c397704e93aea087c1167d54137fa4efaddf392b4`. It was extracted without executing it from the user's existing Classics disc image. No original binaries are intended for publication.

Shipping/diagnostic installation details and hashes are recorded in [the validation receipt](validation/AmbientBehaviorRestorationResults.json). No interactive gameplay or listening comparison was performed. Remaining unverified retail callers and global timing differences are listed in [the updated behavior audit](OriginalPedestrianBehaviorAudit.md).
