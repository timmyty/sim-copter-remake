# Remote taser and automatic pilot recovery

Requested October 10, 2026. These are deliberate remake additions.

Every helicopter has a small yellow-and-black, twin-electrode taser on a gimbal at
the nose. The **REMOTE TASER** cockpit panel is always present, with an **AIM TASER**
button. In flight, that button (or N, or X → Remote Taser → A) transfers flight to automatic hover/follow and
opens the remote helmet sight. No police passenger or equipment purchase is required.
The former officer/passenger camera has been removed.

Move the mouse directly, or use the right stick, to aim. Hold right mouse to release
the cockpit cursor. Left click,
controller A, or RT fires. Roll the mouse wheel for 1x–4x scope zoom; hold LT for
4x and release to restore the previous zoom. Both mouse and stick sensitivity
scale with magnification. Zoom resets on leaving the sight. The centre reticle, persistent control instructions and
READY/RECHARGING readout explain the mode. The button becomes **FLY HELICOPTER**;
click it, press N, or select Fly Helicopter in the X Tools menu and press A to resume manual control. The shot starts at the visible
weapon muzzle, preserves the nonlethal custody rules, has 60 m range and a 1.25 s
recharge. The moving suspect or criminal car remains the pursuit focus. Transport planes do not have
this equipment.

The October 11 control update gives the sight continuous 360-degree horizontal
rotation and vertical aim from -85 to +85 degrees. Yaw wraps smoothly; pitch input
does not accumulate against either limit, so reversing direction responds
immediately. Mouse and controller share these limits. The idle readout shows only
`N / X: Tools`; officer-requirement wording has been removed from the readout, tooltip
and takeoff status.

The pursuit update acquires criminal cars and uncaptured robbers within 60 m,
favors the sight and clear visibility, and preserves a current pursuit until the
player sights another criminal or the target becomes unavailable. A burglar leaving
or re-entering their getaway car can transfer the pursuit between car and person.
The aircraft holds its current position once a clear target is within 35 m. It
keeps that hover until the target is 48 m away or the line stays blocked for 0.8 s,
avoiding constant movement beneath the sight. While following, it leads movement
and chooses a trailing, side or elevated vantage at most once every two seconds
(except when acquiring or resuming a pursuit), with a smoothed approach goal.
The nose heading stays steady during remote movement; returning to flight still
faces the suspect. Full-airframe sweeps still govern
travel. If a target has no clear vantage, it makes a bounded climb and retries;
with no target it holds position and searches. The later [energy taser and free-aim
update](EnergyTaserAndFreeAim.md) keeps the sight under the player's control in world
space, independent of pursuit, and caps gradual approaches at normal sprint speed.
Generic fleeing traffic and getaway cars with completed crime events are excluded.
Mouse travel is independent of frame duration and
uses the profile's X/Y sensitivity; the right stick remains a time-based rate.

## Dropped people and standby medic

Putting down a carried burglar now physically detaches the actor, clears the
shoulder-carry tilt, and restores visibility and collision while retaining custody.
This also fixes the same missing attachment release for airborne drops. A person
just put down is excluded from automatic pickup until the pilot walks more than
1.4 m away and returns; the existing 1.5-second drop cooldown still applies.

A hospital medic may select and walk to the helicopter without an active medical
event when no medic is aboard. The actual boarding action repeats that check,
preventing several posted medics from all taking standby seats. Existing patient
unloading takes priority, and additional medics retain the waiting-patient policy.
The normal physical approach, available-seat and boarding-height gates still apply.

## Pilot-level landing after an airborne exit — October 11, 2026

Without a rescue harness, recovery now waits for the pilot to finish falling,
then searches for clear ground near the pilot. Landing candidates must be at a
reachable height: a safe roof under the helicopter is rejected when the pilot
has fallen to the street. The approach still checks the full airframe for
obstructions and retains its selected spot while descending. If no safe spot is
available, it holds and retries.

The earlier search centered on the airborne exit and preferred the surface
directly below the aircraft. A regression reproduced it parking on a roof above
the pilot. Evidence and installed-file hashes are in
[the steering and recovery results](validation/SteeringRecoveryResults.json).

## Temporary vehicle disable — October 11, 2026

A helicopter taser hit now stops a drivable car for sixteen seconds (doubled by the cage recovery update). Another hit after
the normal recharge refreshes that duration. The car keeps its route and resumes
driving when the timer expires; traffic impulses, separation and blocked-road
recovery cannot move it during the stop. Hits include the vehicle's rotated body
bounds, respect the 60 m range, and stop at intervening people or world geometry.

This is a deliberate remake addition. It causes no damage, fire, arrest credit or
tow mission. Existing breakdowns, tipped cars, wrecks and towed vehicles are not
repaired or reactivated. Ground-agent save payload version 9 preserves the
remaining disable time and continues to read versions 1–8. The on-foot taser
retains its existing suspect-only behavior. No asset or Inspector setup is needed.

Implementation checks and build logs are in `Docs/scratchpad/vehicle-taser/`.
Editor and Shipping builds succeeded. All 14 selected headless regressions passed
with no test warnings, failures or skipped tests, including actual firing, body
hits, misses, range, occlusion, refresh/recharge, traffic holds, automatic recovery,
save round-trip and version-8 compatibility. Build logs retain warnings in
unchanged engine/project code; the test startup also retains the existing missing
`S:/Repos/.../CAPE WELLS.sc2` map warning outside the test results.

The installed Windows executable was updated and hash-verified, with its previous
version backed up under `Docs/scratchpad/vehicle-taser/installed-backup/`. This
code-only update did not require a cook or change assets, saves, cities or radio.
No interactive city playtest was performed. See
`Docs/validation/VehicleTaserResults.json` and the installation receipt in the
scratch directory. The publication checkout has not been synchronized or pushed.

## Automatic pilot recovery

An airborne helicopter holds for two seconds after the pilot exits. The handoff
clears held collective, engine, weapon and winch inputs. Without a purchased rescue
harness it checks the current footprint, then searches nearby clearings within
18 metres of the exit point, nearest rings first. It checks flat, dry support under
the full airframe and a clear approach and descent, avoiding people, vehicles,
other aircraft and world geometry. A new obstruction causes a new search. With no
safe site available it holds position and retries every two seconds. Successful
landing shuts down the engine and leaves the pilot on foot. Search targets are
transient and are safely recomputed after save/load.

On uneven ground, a purchased rescue harness automatically approaches the pilot,
lowers the existing visible rope and harness, and retrieves the person being carried
first. This includes a stunned robber, whose custody remains intact. It then lowers
again and lifts the pilot into the cockpit. With no carried person it lifts the
pilot directly. Passenger capacity and obstruction checks can hold the rescue;
people are not dropped to clear a seat. Save payload version 4 retains a pilot
attached to the harness and reads previous pilot versions.

Walk up to the lowered automatic rescue harness while carrying a patient to load
them automatically within arm's reach. No button press or exact overhead alignment
is needed. A full cabin or intervening obstacle keeps the patient in your arms until
the handoff is possible. The real patient transfers into a cabin seat before the
harness returns for the pilot. **G / controller X** remains an optional handoff at
the harness and the ground-drop control elsewhere.

Every airborne exit starts recovery again. Equipped aircraft switch to harness
recovery when the landing surface is unsuitable, no support is found below them,
or an obstruction blocks the descent to otherwise suitable ground. Walking beyond
18 metres no longer prevents the aircraft from approaching the pilot; the cable's
18-metre limit still bounds its vertical length. Aircraft and cable collision checks
remain active.

Validation for this follow-up: the original code failed eight assertions covering
patient placement, patient-first boarding, repeat retrieval, distant following and
blocked descent. The fixed Editor and Shipping builds succeeded; all 13 focused
headless tests passed without test warnings or skips. Additional checks cover a
full cabin, an unsupported aircraft footprint beside a ledge, automatic patient
pickup, rescue save/resume and blocked cable recovery. The local Windows game
executable was backed up, replaced and SHA-256 verified. No interactive city
playtest or publication checkout update was performed. Evidence:
[Harness patient and follow results](validation/HarnessPatientFollowResults.json),
with detailed logs and installation receipt in `Docs/scratchpad/harness-patient-follow/`.

The subsequent walk-up update reproduces the missing proximity handoff with two
failing baseline assertions. The same real-pawn test now passes without a button
press or an aircraft update between approaching and loading. It also checks
out-of-reach refusal, cabin capacity, an intervening obstacle and automatic retry
after clearance. Editor and Shipping builds succeeded and all 13 focused tests
passed without warnings or skips. The local executable was backed up and
hash-verified after replacement; no interactive city playtest was performed.
[Walk-up results](validation/HarnessWalkUpResults.json) and detailed evidence in
`Docs/scratchpad/harness-walk-up/`.

Damaging dry falls play an original pain grunt. Falls of at least 18 m additionally
play a short generated bone-crack cue. Water and sufficiently slowed parachute
landings remain exempt. Both cues use the game's audio mixer and master volume.

Wind turbines use collision on the actual three rotating blade meshes; gaps
between blades remain open. Demolition/rebuild retains the existing instance
ownership.

Validation results and previews are recorded under
`Docs/scratchpad/remote-taser-recovery/`.

Validated October 11, 2026: Editor and Shipping builds succeeded through the
project wrapper. All 38 distinct regressions passed (31 gameplay/control/save
checks and seven sound checks), with no test warnings, failures or skipped tests.
The final offscreen test also passed and rendered both button states plus all nine
helicopter weapon mounts. It verifies each model switch, including the one-seat
Apache. There was no interactive city gameplay session or listening check.

The Shipping cook, stage, package and archive completed. Slate manifest warnings
are retained in `package.log`. The installer verifies the NonUFS manifest, required
original-game directories and executable hash, backs up changed installed files,
and preserves saves, custom cities and radio. Its exact results are recorded in
`installation.json` beside the build and test reports.

Installed in `C:\SimCopterRemake\SimCopter Remake v1.0.1\Windows` on October 11.
Nine changed files were copied and hash-verified; the NonUFS manifest verified 903
files, 45 existing city files were retained, and all 21 custom radio tracks matched.
Changed originals are backed up in
`C:\SimCopterRemake\SimCopter Remake v1.0.1\Changed-files-before-remote-taser-recovery-2026-10-11`.
The installed executable matches the built Shipping executable.

## Nearby landing and wider aim validation — October 11, 2026

The officer-text cleanup, nearby unattended landing and wider taser controls are
installed in the local Windows game. Editor and Shipping builds passed through
the project wrapper. All 14 selected headless regressions passed with no test
warnings, failures or skips. Added assertions cover nearby landing from an uneven
exit footprint, replanning when a destination becomes occupied, holding and retrying
when support is unavailable, full yaw wrap, both pitch limits and immediate pitch
reversal. Existing harness retrieval, rescue-save obstruction, vehicle taser,
traffic, accident and save tests also passed. Engine deprecation warnings and the
existing startup map-path warning remain in the logs.

Only the native executable changed in the installation; its old version was
backed up and both copies were hash-verified. No recook, asset setup, interactive
playtest or publication-checkout synchronization was performed. Evidence:
`Docs/scratchpad/taser-ui-copy/`, `Docs/scratchpad/nearby-auto-land/` and
`Docs/validation/NearbyAutoLandingAimResults.json`.

## Pursuit, drop and standby validation — October 11, 2026

Editor and Shipping builds passed through the project wrapper. All 20 selected
headless regressions passed with zero test warnings, failures or skips. The
expanded checks exercise a moving criminal car, manual sight offsets during
pursuit, robber/car handoff, fleeing pedestrians, custody release, no-target hover,
reacquisition, a building obstructing the firing line, and equal mouse travel at
30/60 fps. Carry checks verify actual detachment and position after pilot movement,
plus suppression and later resumption of proximity pickup. The original medic
behavior walks aboard without an event and retains the standby seat; a second
posted medic cannot bypass the limit. Medical handoffs/routes, taser stops,
unattended recovery, accidents, parked traffic and save regressions also passed.

The installed native executable was backed up, replaced and hash-verified at
06:54 UTC. Its SHA-256 is
`CEC6B825E1E81695E4F0D96F4F68F499BBFD0D4EBBF7485A5CADE751BF8EAD01`.
Only that executable changed; no recook or asset edits were required. There was
no interactive playtest or publication-checkout synchronization. Existing engine
deprecation and startup map-path warnings remain outside the successful tests.
Evidence: `Docs/scratchpad/taser-pursuit/` and
`Docs/validation/TaserPursuitDropMedicResults.json`.
