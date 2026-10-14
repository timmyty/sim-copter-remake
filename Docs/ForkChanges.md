# Timmyty SimCopter gameplay fork

## October 14 release v1.0.12

Includes all completed changes since v1.0.11: persistent catalog controller
selection; deployed-medic rescue priority, patient seat reservations and cabin
handoffs; the full cockpit radio menu and controller transport/navigation;
the saved remote-taser autopilot switch; nearest-hospital map guidance; the
Turbo minimap lamp/switch and help cleanup; saved bucket/harness restoration;
empty mission-message panel correction; and repaired automation fixtures.
[Release notes](Releases/v1.0.12.md) and
[combined validation](validation/WindowsRelease112Results.json).
## October 13 release v1.0.11

Combines all completed work since v1.0.10: shared lossless original music,
custom-city score/equipment progression and full-map rescue-site selection,
aircraft-sized passenger parties, Photo Mode and day/night sliders, cleaner help
and pause menus, Shift/L3 Turbo, idle-cursor hiding, and native browsing for cities
and saved games. [Release notes](Releases/v1.0.11.md) and
[combined validation](validation/WindowsRelease111Results.json).

## October 13 release v1.0.10

Publishes all completed work since v1.0.9: medical alert pacing, wounded-first
unloading and deployed-medic retrieval; reduced ambient emergency traffic with
nearest street vehicle/HQ dispatch; rooftop exit and crew safety; bounded
responder navigation; stable remote taser aiming, zoom and exit heading; and
explicit orders for each AI helper mission. [Release notes](Releases/v1.0.10.md)
and [combined validation](validation/WindowsRelease110Results.json).

## October 12 release v1.0.9

Publishes all completed work since v1.0.8: full nearby baseball games and the
fixed stadium scoreboard; casualty reactions and parked-helicopter gatherings;
police roof arrests and medical retrieval; airport headings and burglar braking;
original megaphone reactions and footprint-based boat delivery; drive-in music
volume controls; and combined performance/crash diagnostics. Earlier features
remain included. The GPU fault and witness-photo slowdown remain unresolved.
[Release notes](Releases/v1.0.9.md) and
[combined validation](validation/WindowsRelease109Results.json).

Initial publication snapshot: October 8, 2026. This is a customized Unreal Engine 5.8 fork
of [JamesIV4/sim-copter-remake](https://github.com/JamesIV4/sim-copter-remake), built
on [wyozi's version](https://github.com/wyozi/sim-copter-remake/tree/25d81802b065eac5fee1dbd74afb75d1e59004e8).
Original authorship and commit history are retained.

## October 12 combined gameplay release v1.0.8

This release collects the completed hangar reconstruction and equipment previews,
cage car/boat recovery and police-roof deliveries, animated energy taser and remote
aiming, custody-aware crime timers, medical hill routing and destruction aftermath,
automatic pilot landing and harness patient handoff, fleet-wide equipment and
graduation restoration, controller turbo/map/help changes, and victory-band base
confinement. The revised README adds four actual Unreal screenshots showing the
hangar, a car in the sealed cage, the pause-menu entry and the Cheat Codes guide.

[Release notes](Releases/v1.0.8.md) explain the behavior and included Windows
diagnostic launcher. [Combined validation](validation/WindowsRelease108Results.json)
records build, regression, screenshot and package-startup evidence. The existing
GPU compute crash remains unresolved.

## October 11 combined gameplay release v1.0.7

This release gathers all completed changes since v1.0.6: intelligent traffic and
bounded car-only phasing; helper unlocks tied to the current city's graduation;
slower helpers and normal 30% Shift flight boost; remote helicopter tasers,
criminal pursuit and temporary car disable; safe unattended landing and harness
recovery; patient marker/medic route fixes; capture cage controls and transport
clearings; tiered money-cheat odds; dropped-person and standby-medic fixes; and
collision on the actual rotating turbine blades. All earlier work remains in the
source history. [Release notes](Releases/v1.0.7.md) and
[combined validation](validation/WindowsRelease107Results.json).

## October 10 Dispatch transport clearings

Transport pickups and destinations move to nearby dry, level clearings with rotor/tail space and a clear approach. The helper guides waiting passengers to its landing point, uses ground drop-offs, and completes loaded deliveries before returning to base. Existing saved destination markers migrate locally. Patients delivered by the helper resume the original brief celebration and retirement behavior. [Implementation and validation](TransportClearings.md).

## October 10 matching NPC identities and medic care

Walking people, helicopter occupants and passenger portraits now use the same
resolved figure, head and clothing colors. The seated pilot uses the original
pilot appearance in every flyable aircraft. Save/load retains each person's
identity, and hair/helmet colors remain consistent with the portrait.

Medics aboard with a living patient show animated gloved hands providing care in
their portrait until the patient leaves or dies. Existing flight reactions,
impact flinches, patient expressions and the original medic survival benefit
remain unchanged. [Behavior and original-game evidence](NpcAppearance.md).

The publication audit also checked the other recent tasks: their completed work
is already present in `29085c5` or its ancestors. All 393 source/configuration,
project and build-wrapper files match the development copy; tracked assets match
their Git LFS object hashes. [Validation details](ForkValidation.md#october-10-npc-appearance-publication).

## October 10 scheduled combined publication

This update includes the completed work since `754b434` (the restored drive-in
movie), gathered for the requested 8 PM CDT publication. All 391 source,
configuration, project and build-wrapper files match the development copy and
the installed Shipping executable has the same SHA-256 as the validated build.
Earlier October 9 commits remain in history; their features are not counted as
new changes here.

- **City flyovers and turbines:** thirty city-specific routes based on the full
  original CITY*_B movies, with their 30.06366-second timing, close aircraft
  passes, Myrtle Dam wind-farm finale and Calebopolis helicopter tracking.
  Three-blade wind turbines animate in gameplay and paused flyovers. Original
  soundtracks play with focus pause, skip/completion cleanup and radio suspension.
  The source includes the audio extraction/staging workflow; original reference
  movies and decoded soundtrack files remain locally supplied and ignored.
  [Route and soundtrack details](OriginalCityIntroRoutes.md).
- **Shoulder camera and taser:** permanent right-shoulder walking view, closer
  aiming view, and an animated right arm with the taser attached to the hand and
  following aim pitch. [Behavior and checks](ShoulderAim.md).
- **Fire reports and victory band:** at most two full HUD/radio fire reports per
  burst, with a 30-second reset. The original city-dependent band count is
  restored (5–57 requested musicians), with loose clusters, arcs and rows changing
  every ten seconds, personal space, aircraft clearance and obstacle routes.
  [Band and fire details](NpcMedicalAndHelicopterUpdate.md).
- **Medical and rescue safety:** injury-generated medical calls relocate the
  same patient to reachable outdoor ground before publishing the marker. Helper
  aircraft use full-body clearance and safer pickup/delivery; helper-rescue
  failures do not charge the player. Cars check their swept bodies immediately
  before moving around helicopters. Ground-agent save v8 reads v1–7.
  [Placement evidence](ForkValidation.md#medical-incident-placement-follow-up---october-10-2026)
  and [helper safety](CockpitAndSupportSafety.md).
- **Cockpit, aircraft identity and radio:** retractable original wiring panel
  at half its original screen-area share, continuous cockpit trim with existing
  control anchors, career warps retaining the selected helicopter, and level
  starts restoring the chosen station with Rock/KROC as the first-time default.
  [Cockpit details](CockpitAndSupportSafety.md).
- **Airplane controls, sounds and damage:** independently spinning propellers,
  W/S climb/descent, Space/Ctrl throttle, G/GEAR knob and SPEED slider; aircraft
  save v3 retains gear/throttle and reads v1–2. Restore obstacle spring sounds
  and NPC contact conversations. Radioactivity injures 65% of eligible walkers
  while retaining survivors. Apache weapons damage cars and pedestrians;
  missiles collapse full building footprints and settle their fires.
  [Controls and damage rules](AircraftAndDestruction.md).
- **Cheat guide and service signs:** reveal a 20-code guide on the career pause
  menu's unused top plate; accepted movie toggles play the shared cheat feedback,
  including case-insensitive HSI. Remove the police entrance sign while retaining
  its functional service post. [Cheat guide](CheatCodes.md) and
  [service posts](HospitalAndResponderPosts.md).

Fresh combined checks and earlier per-task evidence are separated in
[the validation report](ForkValidation.md#october-10-scheduled-publication-integration).
This source publication does not create a new downloadable Windows release;
release ZIPs remain tied to their tagged snapshots.

## October 10 airplane controls and Apache destruction

Airplane propellers now animate during flight and city flyovers. W/S controls
climb and descent, Space/Ctrl adjusts speed, and G or the dashboard's labeled
GEAR knob raises/lowers landing gear. The matching SPEED slider shows the retained
throttle setting. Gear and throttle survive saves.

Walking into solid obstacles restores the original spring sound; bumping people
starts their original Simlish conversation reaction. Radioactivity injures many
healthy walkers while leaving survivors. Apache missiles explode cars and collapse
complete buildings, including service buildings; machine-gun hits can also destroy
cars. Both weapons injure pedestrians through the medical system.
See [behavior, controls, and original references](AircraftAndDestruction.md).

**This version deliberately departs from the original 1996 gameplay.** In
particular, remotely aiming a nose-mounted taser from a helicopter
while an AI pilot circles the area is a fictional addition. It is not a claim
about authentic SimCopter equipment, original mechanics, or real taser range.

## October 9 pause-menu cheat guide and sound feedback

Hover over the career pause menu's blank top plate to reveal **Cheat Codes** in
the menu's existing style. It opens a scrollable guide to all 20 supported codes.
Continue remains the initial selection; Back/Esc/controller B returns to the pause
menu. HSI/hsi and the other movie starts/toggles now use the shared cheat on/off
sounds after an accepted request. See [the player guide and validation](CheatCodes.md#pause-menu-guide-and-movie-feedback--october-9-2026).

## October 10 medical incident placement correction

New medical calls caused by fire, crime or player collisions now apply the same
outdoor placement checks as scheduled medical spawns. An injured pedestrian inside
a building or on a roof is moved to nearby clear ground before the call is created;
the marker follows the actual patient. Old roof confinement cannot pull them back.
See [the 45-map regression results](ForkValidation.md#medical-incident-placement-follow-up---october-10-2026).

## October 9 patient and road safety fixes

Medical callouts now place new patients on clear outdoor ground, outside building
footprints and rooftops. When no safe nearby position exists, spawning fails instead
of forcing an inaccessible patient into the scene. Parked helicopters stop traffic
that would intersect them, including after the player gets out; vehicles resume
when the aircraft clears the road. This applies to both traffic AI modes and to
emergency vehicles as well as ordinary cars.

These are intentional gameplay safety improvements to the original placement and
traffic rules. Existing rooftop rescue missions and hospital roof handoffs remain.
See [validation and scope](ForkValidation.md#outdoor-patients-and-parked-aircraft-traffic---october-9-2026).

## October 9 combined task publication

The GitHub source now includes the completed work from **Add original cheat code
mechanics**, **Improve vehicles and NPCs**, and **Restore level intros and crash
event**, alongside the previously published witness-photo and hospital-medic fixes.
The runtime source and configuration have been synchronized with the development
build. Required drive-in materials, both prepared movies, authoring tools, regression
tests and player documentation are included.

This supersedes the "unpublished" status recorded in documentation commit
`95c6d89`. That note described the earlier `1a0be52` snapshot, which contained only
the witness-photo and hospital-medic update. The current integration adds the
missing implementations; it preserves both earlier commits and their history.

### Newly included task implementations

- **Cheat codes and original entry UI:** Ctrl+Alt+X opens the original bitmap
  dialog. Restore command matching and flight, pilot, map, career, cash, portrait
  and nuclear effects. The CEO code enables free catalog delivery with keys 1-9.
  See the [full player guide](CheatCodes.md) and
  [implementation notes](memory/simcopter-cheat-codes.md).
- **Drive-in videos, UFO and blast effects:** include the prepared HSI movie
  (19.8 MB, 720p) and original movie upscale, MP4 playback with spatial audio,
  HSI/custom-video/stop commands, package staging, screen materials and preparation
  tools. Restore original UFO geometry/lights and scattered post-blast smoke and
  brief fires. See [movie setup](../README.md#cheat-codes-and-drive-in-videos) and
  [media/UFO notes](memory/simcopter-drive-in-ufo.md).
- **Gort sequence:** original fading artwork, 36 synchronized subtitle/voice pairs
  and left/right audio, the requested 10-second bonus pause, immediate Esc/B/button
  cancellation and automatic return to the same game at 3:06. The original artwork
  is a still image, without mouth animation. See
  [sequence implementation and checks](memory/simcopter-gort-sequence.md).
- **Aircraft occupants, cows and emergency response:** visible pilot/passenger
  bodies, model-specific seating/glazing, cows that bounce without becoming medical
  patients or carryable people, faster emergency vehicles, corrected police routing
  and headquarters coverage for disconnected road components. See
  [aircraft and road coverage](memory/aircraft-passengers-road-coverage.md).
- **Flyable transport planes:** airport aircraft with fixed-wing flight, one fare,
  Transport-only missions, save/load support and the appropriate restrictions on
  emergency passengers, dispatch and equipment. The helicopter fleet/catalog
  remains compatible. Details share the aircraft/road reference above.
- **City intros and accidents:** skippable tours derived from each loaded city's
  scenery, with pause, camera and input restoration. First moving helicopter contact
  tips and ignites a car, reports the original non-mission accident and penalty,
  evacuates occupants and starts a four-second explosion fuse. Includes dousing,
  recovery rules and version-7 saves with older-version support. See
  [city tours and car accidents](CityIntrosAndAccidents.md).
- **Integration checks:** retain all task regression tests and mark the drive-in
  decoder/material test as requiring a renderer, so NullRHI runs exclude it and
  offscreen rendering runs exercise it. The [current validation report](ForkValidation.md)
  records fresh combined results and exact source/build correspondence.

### Earlier October 9 changes retained

[`1a0be52`](https://github.com/timmyty/sim-copter-remake/commit/1a0be5226790b675fd7341d347cec189a539ee35)
added 14 files/changes after `e66675d`: correct daylight exposure and suspect framing
in one-shot witness photographs; purposeful hospital-medic approaches, assigned-post
retention and duplicate-staff prevention; hospital sign removal; regression tests
and validation records. See [witness photos](WitnessPhotoFix.md),
[hospital medics](HospitalGroundMedics.md) and the
[earlier diff](https://github.com/timmyty/sim-copter-remake/compare/e66675d...1a0be52).

### Scope and remaining limits

The feature descriptions above refer to implementations present in this checkout.
The development and publication folders are separate, so future task completion
must include source/asset synchronization and Git publication, not only installation
of a local executable. Original game data and required external build dependencies
remain user-supplied as described in [the build guide](ForkBuild.md).

The October 10 update adds survivor-preserving nuclear injuries and destroyed-fire
cleanup. City tours now follow hand-authored reconstructions of the original full
movies, adapted to the rebuilt scenery; their camera values are not recovered originals. Known test failures
and the lack of an interactive gameplay/listening check remain explicitly documented
in the validation report; this integration does not claim to complete unimplemented
parts of the original game.

## Police tasers and arrests

- The on-foot weapon and helicopter nose-mounted remote weapon are nonlethal tasers.
  Both call `StunForArrest`; neither fires a bullet/missile nor applies a damage or
  death interaction. A successful hit preserves health, immobilizes the living
  criminal and suspends their behavior. Civilians are not valid arrest targets
  and block shots at criminals behind them.
- On foot: hold right mouse / LT to aim, left mouse / RT to fire, within 6.5 m.
  Airborne: the AIM TASER button or N / R3+Y enters remote aim with automatic orbit,
  with no officer required. Move the mouse or use the right stick to aim; right mouse releases the cockpit cursor;
  click, X or RT fires. The fictional airborne range is 60 m.
- Remote mode now says **REMOTE TASER**, with a short blue electrical
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
  have separate persistent responders; their entrance signs have been removed.
- Ground hospital medics admit patients from landed helicopters, the pilot's arms
  or a nearby ground drop, with height, visibility and once-only accounting checks.
- Ambulances recover real deceased NPCs, including crew, and transport their bodies
  to hospitals. Claims, retries and saves preserve recovery progress.
- Pedestrians physically collide with building walls even when the legacy behavior
  asks to move through them; swept movement also handles pushes and overlap escape.
- Marching bands use the original city-dependent count and switch loose formations
  every ten seconds, with spacing and obstacle-aware routes. Dogs are smaller. The cow is a replacement
  procedural black-and-white model with a new gait.
- Doused train wrecks now earn completion credit without double-counting cars.

Details: [mission improvements](MissionImprovements.md), [rescue/pilot fixes](RescuePilotFixes.md),
[NPC medical update](NpcMedicalAndHelicopterUpdate.md), [hospital posts](HospitalAndResponderPosts.md),
[city collision](CitySurfacesAndCollision.md), [cow redesign](memory/cow-visual-redesign.md).

### Air operations, recovery and pilot equipment

- Purchasable swinging tow clamp and four-person capture cage, with scenery
  collision, cabin-capacity checks, actual NPC/vehicle cargo and save restoration.
- The cage adds a matching cockpit flap below the megaphone, permanent skid fittings,
  extending bars and an automatically sealing floor. Owning one increases criminal
  mission weighting; see [capture cage controls](CaptureCage.md).
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
- Airborne helicopter exits now hold briefly, then search for clear flat ground
  within 18 metres and wait safely if blocked. A purchased harness retrieves a carried person first,
  then the pilot, on uneven terrain. Held Space is cleared on exit. P / B toggles
  the parachute; damaging falls play pain audio and severe falls add a crack cue.
- Y opens Tools and switches Tools/Dispatch, A confirms, B cancels; LB/RB yaw and
  both together remain an optional menu shortcut. R3+B exits, R3+Y selects remote
  taser aiming and automatic orbit. Menu navigation cannot move a deployed cable.
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
  marker on foot and dedicated belly spotlight camera.
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

## Fire reports and victory band — October 10, 2026

Fire callouts are limited to two reports per burst with a 30-second quiet period;
all incidents remain available on the map. Victory musicians surround and face the
player, keep their original music and free celebration, and navigate around walls
and parked aircraft. Details and verification are in
[NPC medical and band notes](NpcMedicalAndHelicopterUpdate.md#fire-announcements-and-victory-band-follow-up--october-10-2026).

## Loose victory-band formations — October 10, 2026

Supersedes the fixed circle and 150-second remake choreography. Musicians approach
in loose clusters, shallow arcs and staggered rows, changing every ten seconds.
They leave room around the player/aircraft, reserve separate destinations and
yield to one another. Original music and obstacle routing remain. The original
city-dependent spawn target is restored: five musicians in City0 through 57 in City29,
with the same original spawn-count formula and one leader.
See [behavior and validation](NpcMedicalAndHelicopterUpdate.md#loose-victory-band-and-original-roster--october-10-2026).
