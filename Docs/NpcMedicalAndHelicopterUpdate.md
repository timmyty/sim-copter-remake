# NPC medical incidents, parade band and helicopter presentation

Implemented 2026-10-05 in the Unreal 5.8 project. These gameplay changes are requested deviations from original SimCopter behavior.

## Changes

- Pedestrian movement checks live building flames, burning vehicles/wrecks and burning debris, with a safety margin. Contact converts the existing living NPC into a medical patient and creates a MedEvac mission. It does not duplicate the victim, charge the pilot an injury fine, or repeatedly create missions for the same patient. Heights are checked so roof fires do not injure people on the street below.
- BHAV 1175's mugger attack (record 4, pushed reaction 903) now creates the same medical incident instead of running the death program. Other causes of injury/death retain their existing behavior. Patients still need timely medical care.
- Musicians use a bounded local A* search with ground, water, fire and geometry checks. They alternate 150 seconds in formation with 150 seconds of original BHAV 443/444 celebration behavior. Formation periods cycle through a line, two columns and a chevron. Instrument animation and original sound behavior continue. Injured or carried musicians stop taking formation movement commands. Restored band actors are reused after loading.
- Active medical incidents prepare the nearest hospital with 4-8 roof staff. A loaded aircraft also prepares its nearest hospital according to patient count (up to eight staff, a majority even for a fourteen-passenger aircraft). Other hospitals retain a medic. Existing crew are counted before spawning replacements. Patient reservations persist while BHAV 263 temporarily selects the aircraft, allowing multiple medics to retrieve different people concurrently through the existing carrier service.
- Dog figure geometry is one third its previous size in every animation clip.
- A burning train wreck posts `EVT_CarDoused`, but the completion award previously read only `CarsCleared`. Completion now credits `max(CarsCleared, CarsDoused)`, so extinguished wrecks receive credit and regular cars that post both events are not counted twice. Burning out still earns no saved-wreck award.
- Helicopter paint palette ramp bases now use a brighter swatch before scene lighting. Literal black, individual ramp shades, tires and effects are preserved. The original AGUSTA body uses palette 48 `(15,15,15)` and 176 `(21,15,16)` extensively, explaining the nearly black appearance in the former literal-palette conversion.
- The Agusta's cockpit and cabin window faces are separated into a translucent material. Seated occupant geometry follows the pilot's possession and actual passenger manifest; patients have bandages. Boarding, unloading, parking and model changes update the cabin. The interior view hides these exterior representations along with the fuselage.
- All eight catalog sheets have expanded History, Specialties and Description text, retaining original catalog specifications. The text panels scroll within the existing hangar page.
- On foot, a cyan **Your helicopter** marker identifies the aircraft just exited. The save restoration path relinks the same aircraft. Boarding removes the parked marker.

## Ground truth and historical sources

Original BHAVs and mesh/animation data remain in `Reference/SimCopterOriginalGame`. The existing source documents BHAV 443/444 (band), 263/282 (hospital handoff), 1175 (mugger), `FUN_004a73e0` (mission lifecycle), and `FUN_004aabf0` (completion scoring). Original catalog rows establish the eight model identities and specification figures; the expanded prose does not retune aircraft performance.

Supplemental historical references used for the expanded biographies:

- [Schweizer manufacturer history](https://schweizerrsg.com/pages/about-schweizer)
- [Bell: JetRanger solo circumnavigation](https://news.bellflight.com/en-US/228748-40-years-since-the-world-s-first-solo-circumnavigation-by-helicopter-used-bell-206)
- [Bell: US Park Police aviation history](https://news.bellflight.com/en-US/231718-u-s-park-police-celebrates-half-a-century-flying-bell-aircraft/)
- [Boeing: transfer of the MD light-helicopter product line](https://boeing.mediaroom.com/1999-02-19-Boeing-Completes-Sale-Of-Light-Helicopter-Product-Lines-To-RDM)
- [MD Helicopters: MD 520N technical description](https://support.mdhelicopters.com/files/Models/MD520N_Tech_Desc.pdf)
- [Leonardo: A109 fiftieth anniversary](https://www.leonardo.com/documents/15646808/16755140/PressNote_A109_50thAnniversary_1stFlight_04_08_2021_ENG.pdf?t=1628071306713)
- [Airbus helicopter history](https://www.airbus.com/sites/g/files/jlcbta136/files/2021-09/en-Book-80-years%20%284%29.pdf)
- [Airbus: Fenestron development](https://www.airbus.com/en/newsroom/stories/2022-07-safety-innovation-2-the-fenestron)

## Validation and delivery

Evidence is under `Docs/scratchpad/npc-medical-update/`. Source changes are compared against the pre-change source snapshot in `source.diff`; this supplied project has no Git metadata.

The new `SimCopter.NpcMedical` automation tests exercise attack conversion, fire-contact conversion and avoidance, duplicate-event prevention, train-wreck completion, distinct patient reservations, simultaneous carrier transfers, bounded trauma-team spawning, band phase changes and obstacle routes, dog scaling, actual Agusta model/glass loading, manifest-driven occupant geometry, and the parked marker.

Offline previews render the actual Unreal-generated Agusta mesh sections exported by the test. They confirm the window openings and occupant placement from both sides. They are geometry previews, not Unreal lighting or gameplay screenshots. No interactive game session was launched; visual timing, hospital crowd movement, lighting and the hangar scrolling still need an in-game check.

Final Editor and Shipping builds passed through `RebuildUnrealCpp.bat`. The final headless run executed 77 tests: 75 passed cleanly, two passed with warnings, zero failed or skipped. The warnings come from the existing `PlaneDeckRescue` fixture's component mobility and `SafePassengerLanding` fixture's contextless synthetic world; neither indicates a failed assertion. `tests-final/index.json` and `tests-verified.json` record the results, including the actual burning-vehicle height regression.

The final Shipping cook/package/archive succeeded. Its IoStore index includes `M_SimCopterCabinGlass.uasset`; the NonUFS manifest verified 869 files and all six required original-game data directories. Existing cook/staging warnings are retained in `package.log`.

Installed into `C:\SimCopterRemake\SimCopter Remake v1.0.1\Windows` on 2026-10-05. The installer updated and hash-verified ten files, preserved 45 local city/save files, and verified all 21 custom radio tracks against the previous installation. A complete backup is at `C:\SimCopterRemake\SimCopter Remake v1.0.1\Windows-before-npc-medical-update-2026-10-05`. See `installation.json` for the exact executable, file hashes and timestamp. No live game session was launched.

## Fire announcements and victory-band follow-up — October 10, 2026

Fire reports now admit at most two complete announcements per burst, followed by a
30-second quiet period after the last admitted report. The same decision controls
the HUD message and all four radio phrases. Suppressed reports are discarded, not
queued for later; all fire missions and map entries remain active. Other incident
reports and completion/scoring messages are unaffected. Throttling resets for a
new session or load without changing the save format.

The earlier remake band formed around its own starting centroid for 150 seconds. It
now spawns around the nearby player or the nearest airport pad while the player
is away, then updates individual positions around the player once a second.
Musicians face inward, allow space for the helicopter, and try nearby open ground
if their sector is blocked. The original BHAV 443/444 march speed, instruments,
and the remake's alternating 150-second free-celebration intervals remain. The original
approach now completes at each musician's ring position so instrument routines
can proceed without requiring contact with the helicopter.

The bounded local A* search can connect to a distant visible target after routing
around a wall, smooths only physically clear segments, replans stale/blocked paths,
and checks building geometry, terrain, water, fire and aircraft parking bounds.
Aircraft above pedestrian height do not block the ground route. Medical state
changes immediately release formation control, and saved musicians are reused.

Editor and Shipping builds passed. All 70 selected headless tests passed (68
clean, two with existing fixture warnings). The new fire-burst, player-circle and
long-route tests all failed when their corrected behavior was temporarily reversed,
then passed with the fixes restored. Aircraft routing, blocked ring slots, player
movement, injury release and unchanged save layout are covered. The aircraft
fixture was grounded using its actual bounds after its first setup left it above
the pedestrians. No interactive gameplay or listening check was performed.

The executable/PDB and their two installed manifests were updated with verified
backups and SHA-256 checks in `SimCopter Remake v1.0.1/Windows`. Source was copied
into the local publication checkout, preserving unrelated changes; this task did
not create a Git commit or push. Evidence: `Docs/scratchpad/fire-band/`; compact
results: [FireBandResults.json](validation/FireBandResults.json).

## Loose victory band and original roster — October 10, 2026

This revision supersedes the circle and 150-second cadence above. Musicians approach
in a loose cluster, shallow arc, and staggered rows, changing shape every ten seconds.
They retain the original walking speed, playing animation, leader march and individual
brass sounds. Positions follow the nearby player, with a 250 cm minimum buffer (larger
for the occupied helicopter). Paths avoid that buffer as well as existing terrain,
walls, fire, water and parked-aircraft obstacles. Reserved destination alternatives
and local yielding/sidestepping keep musicians from converging on the same spot.

The original does **not** use a fixed eight musicians. The exported original
`FUN_004c2a20` calls `FUN_004c29d0` with `cityIndex / 2 + 5` people, repeated
`max(1, cityIndex / 8)` times. `FUN_00408210` confirms that `DAT_00518d64` is the
zero-based career city index, not the career level. Thus the routine requests five
spawns in City0, eight in City6, 26 in City16 and 57 in City29 (three groups of 19
at randomly selected airport pad tiles). These are spawn attempts, not guaranteed
visible counts: `FUN_004c2ef0` can exhaust the shared 500-person pool, and
`FUN_004c4190`/`FUN_004c02a0` can reject placement after two candidate checks.
The current remake draft uses that requested total and retains one leader;
retrying until full is therefore stronger than the original's best-effort spawn.
Wider/deeper formations accommodate the larger roster.
Saved bands are reused and resized on restoration; failed safe placements retry
without duplicating existing people. Medical state changes still release guidance.

Primary evidence and validation logs are under `Docs/scratchpad/band-revamp/`.

Editor and Shipping builds succeeded. All 70 targeted tests passed (68 clean and
two with existing fixture warnings), including actual roster restoration and
movement checks. The installed executable/PDB and two manifests were backed up,
updated and hash-verified. Seven source files were synchronized locally; no push
or interactive gameplay/listening check. See [results](validation/BandRevampResults.json).

## Patient markers and deployed medic routes — October 11, 2026

Patient pickup markers now anchor to each live casualty's actor position and elevation,
instead of the mission tile center. They follow relocated patients, distinguish multiple
patients in one call, and disappear when the person is resolved or carried. Remaining
ground patients stay marked when another patient from the same call is aboard.

Deployed medics reserve separate available casualties and use the existing bounded
physical A* planner to get around obstructions, including routes that initially lead
away from the patient. Paths recheck obstacles and replan for moved targets. The probes
respect geometry, step heights, fire, water, hospital posts, parked aircraft and nearby
people. The target aircraft remains approachable for boarding. Failed routes release
the patient claim and retry after a delay. Original BHAV 801/262 selection flow, movement
speed, contact checks, pickup and handoff remain in control; route state is transient.

Before the fix, exact patient-marker checks failed four assertions. Reversing the medic
approach and reservation fixes reproduced five failures in a U-shaped enclosure: one
medic and all three medics stayed trapped, with the group selecting the same casualty.
With the fix, all four medics in those two scenarios reached their separate targets
without crossing the wall, and the shipped BHAV program completed a patient pickup.

Editor and Shipping builds succeeded. All 75 selected headless tests passed: 73 clean
and two with existing PlaneDeckRescue/SafePassengerLanding fixture warnings. The final
run followed a full consistent Editor rebuild after overlapping earlier compilations
produced an invalid intermediate test run. The validated executable, symbols and two
manifests were installed with rollback backups and SHA-256 verification. Task source
was synchronized to the local publication checkout, preserving other pending changes.
No commit, push, or interactive gameplay/visual check was performed.

Evidence: `Docs/scratchpad/patient-markers-medic-paths/` and
[validation results](validation/PatientMarkersMedicRoutesResults.json).

## Band confinement to the hangar base — October 12, 2026

The greeting now stays inside the airport's four-by-four-tile footprint, with a
body-width margin. Previously the listener could draw the band up to four tiles
beyond an airport pad, and formation offsets and stand-location retries extended
it farther. Both destination selection and every walking/pathfinding segment now
respect the base. The boundary uses the same three-pad basis as hangar placement,
so it follows translated, rotated and scaled cities rather than world axes.

Musicians follow the player while on the base and gather at the nearest pad when
the player leaves. Large rosters and blocked slots search spaced, physically clear
alternatives on the apron. Newly spawned members immediately receive guidance;
outside stragglers can walk inward without teleporting. Original BHAV 443/444
music, march speed, formation cadence and injury/carry release remain intact.
The original airport-spawn evidence is FUN_004c2a20/004c29d0; confinement is the
requested remake adaptation, not a claim about the original behavior.

Nineteen assertions reproduced the unrestricted movement before the correction.
All 21 focused headless tests pass, including the 57-member roster, every formation
with listeners beyond each side, actual outward/return walking, aircraft avoidance,
medical navigation, saves, and airport/people rules. Existing formation fixtures
were moved onto the base so their movement/cadence assertions still exercise free
space. Rotated/scaled and fallback-airport boundary calculations are also covered.
Editor, Shipping and Development wrapper builds succeeded. Both installed
executables and symbols were backed up, replaced after the user closed the game,
and hash-verified. No interactive gameplay or listening check was performed.

[Validation and installation](validation/BandBaseConfinementResults.json).
Evidence and rollback copies: `Docs/scratchpad/band-base/`.

## Airport victory band retention — October 11, 2026

Completing the score requirement across town could spawn the airport band and
immediately lose it: ambient traffic cleanup removed the distant musicians,
whose mission-event ID is empty, while the band's completed-spawn flag prevented
replacement. Musician states 17/18 are now exempt from ambient distance cleanup.
The airport roster is rechecked once per second, reusing survivors and filling
missing places, including a missing leader.

The band still spawns at the airport and greets the returning player there.
Landing elsewhere does not move its greeting site. Existing approach, audio,
safe-placement, and progression rules are retained: Islandtown starts with three,
the next cities have five and six, and the final city's roster reaches 57. This
also repairs an already-completed saved city when its musicians are missing.

`SimCopter.NpcMedical.BandClusterAndSpacing` reproduces the original distance-cull
failure and lost-leader recovery before exercising existing formation/roster
checks. The regression now passes. This validates spawning and retention, not
an interactive music/listening session. Evidence is in
`Docs/scratchpad/equipment-preview/` and
[the combined results](validation/EquipmentPreviewAndAirportRecoveryResults.json).

## Steep-hill patient return — October 11, 2026

The physical route planner used the live medic's feet height when probing every
distant point, and compared a 35 cm horizontal sample against the five-unit climb
limit. Continuous steep ground could therefore look impassable. Route probes now
advance from the preceding supported height in smaller increments, and A* retains
each node's supported elevation through detours and path smoothing. The same
planner serves deployed officers and the band; wall, overhead, water, fire and
per-step climb checks remain enabled.

The regression carries the actual patient through shipped BHAV 262 up a ten-metre,
45-degree hill, including a wall detour. Two free seats admit patient and medic;
one admits the patient alone. A full cabin cannot overfill or destroy the patient;
the original medic program may set the patient down. Downhill probing and 15/60 Hz
updates are covered. No movement-speed increase or teleport was added for medics.

Editor and Shipping builds and all 35 selected headless tests passed. One hangar
fixture reports two existing missing optional flap-art warnings. The executable
was installed with backup and hash verification. No interactive playtest or
publication checkout update was performed. See
[combined results](validation/MedicPoliceAndHangarResults.json).
