# Timmyty SimCopter gameplay fork

Publication snapshot: October 8, 2026. This is a customized Unreal Engine 5.8 fork
of [JamesIV4/sim-copter-remake](https://github.com/JamesIV4/sim-copter-remake), built
on [wyozi's version](https://github.com/wyozi/sim-copter-remake/tree/25d81802b065eac5fee1dbd74afb75d1e59004e8).
Original authorship and commit history are retained.

**This version deliberately departs from the original 1996 gameplay.** In
particular, controlling a police passenger and firing a taser from a helicopter
while an AI pilot circles the area is a fictional addition. It is not a claim
about authentic SimCopter equipment, original mechanics, or real taser range.

## Police tasers and arrests

- The on-foot weapon and helicopter police passenger weapon are nonlethal tasers.
  Both call `StunForArrest`; neither fires a bullet/missile nor applies a damage or
  death interaction. A successful hit preserves health, immobilizes the living
  criminal and suspends their behavior. Civilians are not valid arrest targets
  and block shots at criminals behind them.
- On foot: hold right mouse / LT to aim, left mouse / RT to fire, within 6.5 m.
  Airborne with a living officer aboard: N / R3+Y enters police taser mode; mouse
  or right stick aims; click, X or RT fires. The fictional airborne range is 60 m.
- Police passenger mode now says **POLICE TASER**, with a short blue electrical
  discharge matching the on-foot effect. The old “sniper” wording and crash/dust
  impact effect have been replaced. This publication also clears queued aircraft
  fire on mode changes and blocks Apache gun/missile tool commands and held-gun
  emission while the taser role is active.
- Walk up to the stunned person to carry them; boarding a helicopter handcuffs
  them and shows cuffs on their portrait. A police-station roof officer receives
  prisoners individually; arrest credit is awarded at delivery, not on shooting.
  Successful delivery removes the actor after its custody presentation; it does
  not mark the prisoner dead. Custody persists in saves.
- The game's separate Apache weapons and other hazards still exist. These taser
  changes do not make crashes, fire, dangerous falls or all gameplay nonlethal.

## Local project changes since the wyozi baseline

The sections below describe the final behavior, including follow-up corrections
that supersede earlier implementation notes. The [file inventory](ForkFileInventory.md)
records every added or modified publication file relative to the local baseline.

### Mission pacing and gameplay flow

- Restore Base Location to the mission-cap count: tier 1 normally offers one job,
  with an additional job waiting the full 380-second interval; higher tiers still
  increase availability. This reverses wyozi's base-slot exclusion.
- Retain the inherited relaxed mission clocks, increased arsonist activity and
  viable riot population minimum; these are documented gameplay divergences.
- Continue appears first in the pause menu. Losing application focus clears held
  inputs and pauses; returning opens Continue without stealing another menu's pause.
- Completed levels offer **Next Level** inside the hangar, with duplicate-award
  protection. Enter outside the hangar no longer advances the career.
- Crash rescue sirens stop after three simulation seconds and clean up on recovery.
  Emergency vehicle sirens have approximately half gain and a 60 m rather than
  120 m falloff range. A new fuel warning sounds at 10%, rearming above 12%.

Details: [Playable build](PlayableBuild.md), [gameplay polish](GameplayPolish.md).

### Rescues, medical care and pedestrian behavior

- Transport passengers can complete delivery on the correct destination's supported
  flat roof. Wrong roofs and airborne releases do not count.
- Aircraft and shipwreck rescues place actual survivors on the wreck geometry.
  Hull footholds follow translation/tilt and reconnect after saves; occupied wrecks
  cannot be towed. Rescue groups use six distinct portrait classes per event, and
  restored cabin portraits refresh from the actual people aboard.
- Passenger exits reserve separate clear spaces on both sides of the fuselage and
  retry when blocked. Mission, helper, manual and police handoffs share this rule.
  The pilot's exit side remembers the last directional input. Harness grip is raised.
- Automatic pickup within 90 cm collects stunned suspects, patients and recoverable
  bodies, with line-of-sight, height, fall, capacity and drop-delay checks.
- Fire avoidance and contact injury convert the same living NPC into a MedEvac
  patient. Mugger attacks now injure instead of executing the death program.
  Timely medical treatment still matters.
- Trauma staffing scales to nearby medical demand, with reservations preventing
  multiple medics from claiming one patient. Roof staff use supported, separated
  positions and retry instead of stacking. Hospital and police ground entrances
  have separate persistent responders and signs.
- Ground hospital medics admit patients from landed helicopters, the pilot's arms
  or a nearby ground drop, with height, visibility and once-only accounting checks.
- Ambulances recover real deceased NPCs, including crew, and transport their bodies
  to hospitals. Claims, retries and saves preserve recovery progress.
- Pedestrians physically collide with building walls even when the legacy behavior
  asks to move through them; swept movement also handles pushes and overlap escape.
- Marching bands alternate 150-second formations with original celebration behavior,
  using obstacle-aware local routes. Dogs are smaller. The cow is a replacement
  procedural black-and-white model with a new gait.
- Doused train wrecks now earn completion credit without double-counting cars.

Details: [mission improvements](MissionImprovements.md), [rescue/pilot fixes](RescuePilotFixes.md),
[NPC medical update](NpcMedicalAndHelicopterUpdate.md), [hospital posts](HospitalAndResponderPosts.md),
[city collision](CitySurfacesAndCollision.md), [cow redesign](memory/cow-visual-redesign.md).

### Air operations, recovery and pilot equipment

- Purchasable swinging tow clamp and four-person capture cage, with scenery
  collision, cabin-capacity checks, actual NPC/vehicle cargo and save restoration.
- Generated auto repair yards, junkyards and shoreline harbor repair sites accept
  compatible cargo. Recovered hulls and stalled vehicles have distinct mission names.
- An AI helper unlocks after completing a level, responds automatically or to the
  selected mission, transports actual people/cargo, and returns on recall. It stays
  separate from the player's owned/sellable aircraft and ignores Base Location as a job.
- Helicopter/car contacts produce local dents, an original impact sound and sparks.
  A second distinct impact stalls the car; a fourth ignites it and causes a delayed
  explosion. Player-created wreck/recovery jobs cannot farm rewards.
- Ambient breakdowns are limited to one every 15 minutes without towing equipment;
  equipped aircraft retain the faster 150-second interval and two-car limit.
- Airborne pilot exits trigger the existing helicopter crash descent. P / B toggles
  a visible parachute with gradual braking; last-moment deployment does not erase
  impact injury. Water and dry falls use their respective medical rules.
- Y opens Tools and switches Tools/Dispatch, A confirms, B cancels; LB/RB yaw and
  both together remain an optional menu shortcut. R3+B exits, R3+Y selects police
  taser mode. Menu navigation cannot move a deployed cable.
- On-foot taser mesh, canopy, health, helper assignments, cargo, custody, recovery,
  impacts and relevant progression are integrated into versioned saves.

Details and controls: [Air operations](AirOperations.md), [rescue/pilot fixes](RescuePilotFixes.md),
[stalled vehicles](RadioVolumeAndStalledVehicles.md).

### Helicopters, cameras, world art and interface

- Model-specific civilian paint schemes follow the eight original catalog drawings;
  the Apache stays olive. Satin/matte materials add local surface variation.
- The Agusta has translucent cabin glass and occupants driven by actual possession
  and the passenger manifest, including patient bandages.
- A ninth mystery catalog card reveals the Apache in original late-game maps
  City25–29. Boarding claims the discovered aircraft; paid delivery costs three
  times the most expensive civilian model (61,500), includes its existing weapons,
  prevents duplicate encounter/ownership exploits and persists across saves.
- Expanded helicopter catalog histories/descriptions, saved **Your helicopter**
  marker on foot, dashboard wind readout, and dedicated belly spotlight camera.
  The camera hides its own airframe and restores it on leaving the view.
- Buckets can refill from authored pools/ponds and rendered water beyond map bounds,
  without making surrounding land a water source.
- Robber events provide a one-time photograph of the actual suspect, coordinates
  and a queued spoken witness briefing. Police dispatch recalls the nearest
  eligible unit after an arrest.
- City surface filtering/mipmaps avoid atlas-cell bleed, with land/masonry detail.
  Hospital cladding, roof and helipad H use dedicated procedural presentation while
  preserving original geometry/collision.
- New procedural UFO with generated hull art, canopy and luminous rings, retaining
  existing flight/abduction behavior. New Windows SIM COPTER helicopter/city icon.
- Marker layout measures long labels and distance text, including **Stalled vehicle**.

Details: [aircraft colors/Apache](HelicopterColorsAndApache.md), [rescue/pilot fixes](RescuePilotFixes.md),
[city surfaces](CitySurfacesAndCollision.md), [hospital presentation](HospitalAndResponderPosts.md),
[gameplay/UFO](GameplayPolish.md), [mission improvements](MissionImprovements.md).

### KINV music and audio controls

- All 21 user-supplied **Invoke the Revoked** tracks are included, explicitly
  authorized for this public fork. KINV plays the eight Dancing to Our Doom album
  tracks first, then the remaining thirteen, without DJ interruptions or shuffle.
- Track and seek position persist across station switches, power-off and sessions.
  Radio volume spans 0–200%; tuning preserves gain/mute and all normal settings-menu
  exits save changes. Auto-Quiet retains its existing multiplier.
- PCM WAV payloads, witness speech, fuel/impact cues, custom art and Unreal assets
  use Git LFS. The [playlist manifest](InvokeRadioTracks.json) records durations
  and SHA-256 hashes. Local workstation source paths are omitted from publication.

Details: [KINV](InvokeRadio.md), [volume persistence](RadioVolumeAndStalledVehicles.md).

### Build and maintenance

- Windows wrapper locates UE 5.8 in the standard Epic Launcher installation and
  supports the Shipping target; retain `-NoLiveCoding` and non-unity compilation.
- Fix the benchmark helper's shadowed `Controller` variable for MSVC and normalize
  a settings-test INI path; add regression tests for the local gameplay changes.
- Retain reproducible material-generation tools and authored asset sources.
  Original commercial game data, locally baked derivatives, NVIDIA SDK plugins,
  personal saves, caches and build outputs are not added to the source fork.
- [Build instructions](ForkBuild.md) describe Git LFS, original data and required
  local dependencies. [Validation](ForkValidation.md) states what was actually tested.

## Inherited changes and attribution

The verified local source baseline is wyozi commit
`25d81802b065eac5fee1dbd74afb75d1e59004e8`, which is 31 commits beyond JamesIV4
`11977d3b08e17c4be0a07eb62e70fdd866e8afcf`. The initial downloaded 4,427 files were
verified against that baseline before local work. These 31 commits are retained
with their original authors, not reattributed to this project.

Inherited work includes Apple Silicon build/packaging, Mac input/focus fixes and
performance profiles, shared GEO parsing, benchmark tooling/timing scopes, cloud
tuning, current-camera marker placement and marker settings, cockpit sound
muffling, original map selection/Base Location behavior, transport marker state,
real-time time of day, delivery to any hospital, riding-medic unloads, graphics
settings synchronization, and debug tools. Local base-slot pacing intentionally
supersedes inherited commit `430e445978db`.

The complete inherited commit list appears in [ForkFileInventory.md](ForkFileInventory.md).
The original game and original remake's existing features are credited to their
respective creators; this fork does not claim them as new local work.

Earlier feature documents retain historical build/install results and local
scratchpad references. Those references may not exist in a clean public clone;
they are not claims that this publication was manually played through. The
publication validation report is the current verification record.
