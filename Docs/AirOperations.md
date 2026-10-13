# Air operations, recovery and police update

Implemented 2026-10-05. These are user-requested additions to the original SimCopter behavior.

## Controls

| Action | Controller | Keyboard / mouse |
| --- | --- | --- |
| Rotate helicopter left / right | LB / RB individually | Existing flight controls |
| Open Tools | Press **X** | Existing tool panel |
| Switch Tools / Dispatch | Press **X**, no bumper hold required | Click the corresponding panel |
| Select and confirm | Right stick, then **A** | Click selection |
| Cancel wheel | B | Existing panel controls |
| Passenger menu | Select Passengers in Tools, then A | Click a seat portrait |
| Recall ground services and helper | Select Recall All in Dispatch, then A | Recall from Dispatch |
| Select capture cage | Tools wheel | V |
| Use selected tool / dump bucket / deploy cage | A | R or left click |
| Raise / lower sling | D-pad up / down | Page Up / Page Down |
| Open cage / deliver over active recovery ring | D-pad left | G or OPEN |
| Enter / leave remote taser mode | X opens Tools; select Remote Taser / Fly Helicopter, then A | N or cockpit AIM TASER / FLY HELICOPTER button |
| Aim / fire in remote taser mode | Right stick / A or RT | Move mouse / left click; hold right mouse for cockpit cursor |
| Remote taser scope zoom | Hold LT for 4x; release to restore zoom | Mouse wheel adjusts 1x–4x |
| Aim / fire taser on foot | LT / RT | Right mouse / left mouse |
| Carry downed person | Walk up; automatic | Walk up; automatic |
| Enter helicopter on foot | Y (or walk close) | F (or walk close) |
| Toggle Sprint Mode in helicopters / on foot | Click L3 | Hold either Shift key |
| Show / hide help in helicopters / on foot | Hold R3 + click L3 | — |
| Exit helicopter | Y | F |
| Toggle parachute while falling | B | P |
| Put carried person down | X | G |

X opens Tools and toggles between Tools and Dispatch. Outside a menu, A uses the selected tool; hold A for continuous actions such as dumping a bucket or spraying water. RT handles climb/start independently of A. The menu stays open without holding any button; A confirms and B cancels. Both bumpers together remain an optional hold-and-release shortcut. They suppress helicopter turning while the wheel is open; the remaining held bumper stays suppressed until both have been released, preventing an accidental turn. Y enters/exits the aircraft, and walking close still boards automatically. Remote Taser / Fly Helicopter and Passengers are Tools options; Recall All is in Dispatch.

## Recovery

Buy a **Capture Cage ($1,800)** from the hangar equipment catalog. This per-aircraft fitting replaces the tow clamp; legacy purchases upgrade automatically. It extends from 1.4 to 18 meters, with clearance for the carried object's height.

Career advancement retains the helicopter the pilot last used and its purchased equipment, including the capture cage, even when choosing Next Level on foot. The airport plane and support helper cannot replace that aircraft in the transfer.

Junkyards occupy suitable clear, flat ground beside roads. Harbor sites occupy clear shoreline water and contain a crane, deck, and hull support cradle. Sites are generated for the current city; inland cities have no harbor. Their small minimap markers grow slightly when relevant missions activate. Active destinations also have pulsing world-space light rings. The cargo HUD reports the matching site's bearing and distance.

Lower the cage over a stalled, fleeing or taser-disabled car, or a capsized hull. Its floor seals automatically. Bring cars to the junkyard ring and boats to the harbor ring; slow down, lower gently, and press **OPEN / G** to deliver. Recovered vehicles remain on the platform briefly. Capsized boats require all riders to be rescued before hull pickup. See [cage recovery and waypoints](CaptureCage.md#vehicle-recovery-and-mission-waypoints--october-11-2026).

## Arrests, cages and falls

Aim the taser on foot near a visible active criminal within 18 meters. Its circuitry lights and its muzzle opens to form an energy orb; click or press RT to release the formed orb. It seeks the aimed criminal and stuns them on contact for 12 seconds, or stops a criminal car for sixteen seconds. The stored orb fades after 30 seconds without another sighting. See [energy taser and free remote aiming](EnergyTaserAndFreeAim.md). Walk within 90 cm of a stunned suspect to pick them up and handcuff them, then carry them into a helicopter with a free seat. Injured people and recoverable bodies use the same pickup behavior. Pickup requires a clear path and similar height, and does not happen while the pilot is falling. Putting someone down leaves a 1.5-second pickup delay. Criminal occupants display silver handcuffs with interlocking chain links and translucent prison bars over their passenger portraits. Hover slowly near a police-station roof: an officer takes custody of one prisoner every 1.5 seconds. Arrest mission credit is awarded on delivery.

The purchased cage has its own cockpit panel immediately below the megaphone, using the same original frame, grid, button art, colors and HUD scaling. Hold **LOWER** to form the cage directly from its visible skid fittings and lower it around people. Its bottom grille seals automatically when it captures up to four NPCs; the readout shows forming, open, sealing and occupant count. Hold **RAISE** to retract, then keep holding for another 1.25 seconds to board occupants. Cabin capacity applies; excess occupants remain in the cage. **OPEN** releases everyone still inside. The empty cage stows at the top, leaving its compact fittings visible on the aircraft. Existing V / Page Down / Page Up / G and controller controls remain available.

Owning a cage on any player aircraft increases the scheduler's crime-family weight by three times, with a minimum based on the city's average category weight so early cities also offer criminal jobs. This affects the existing robber, mugger, arsonist and speeder mix; mission limits and other jobs still apply. The helper's free cage does not qualify. Selling the final owned cage restores the original weights, and all-zero/free-roam settings remain quiet. See [capture cage follow-up](CaptureCage.md).

Released NPCs use the existing falling and medical systems. Water landings create a MedEvac incident without impact damage. Dry falls of 9 meters or more inflict health damage and can create a medical incident. The pilot can also leave an airborne helicopter and suffers fall damage; an incapacitating pilot injury triggers recovery at the aircraft after five seconds.

## AI helicopter and remote taser mode

Passing a level unlocks a second helicopter that waits for your orders. Select a mission on the map, then choose **AIR: MISSION** in the cockpit Dispatch panel or **Air: Map Mission** in the controller Dispatch wheel. Each order authorizes that one mission; after completing it, or if it becomes unavailable or unreachable, the helper returns to base and waits. It never chooses the next mission on its own, and the former **AIR: AUTO** option has been removed.

Each new city, including a career cheat warp, requires graduation again before the helper appears. Recall returns it toward base after completing any loaded passenger or cargo delivery. The helper cruises at 600 cm/s and uses real NPCs, vehicles, mission events, and service destinations. It cannot be entered or sold by the player. Older saves retain explicit mission orders; legacy automatic assignments are recalled, with any loaded people or cargo delivered first.

Click **L3** to toggle Sprint Mode, or hold either **Shift** key, for 30% faster horizontal helicopter travel with a steeper flight tilt. On foot, L3 toggles the existing doubled running speed and animation. **R3+L3** shows or hides controller help; the sprint status remains visible. Boarding, exiting and loading clear the toggle. With `superpowermultiply` active, either input retains its existing turbo/dog behavior without stacking multipliers. See [Sprint Mode and GPU diagnostics](GpuCrashAndSprint.md) and [traffic and flight pacing](TrafficFlowAndFlightPacing.md).

Every helicopter has a visible nose-mounted remote taser. Once airborne, click **AIM TASER** on its cockpit panel press **N**, or open Tools with **X**, select **Remote Taser**, and press **A**. No officer or purchase is required. Autopilot holds a steady hover while the suspect is in range with a clear shot, following again when range or sustained cover requires it. Move the mouse or right stick to aim; roll the mouse wheel for 1x–4x zoom, or hold LT for the 4x scope. Aiming sensitivity reduces with magnification. Left click, A or RT fires. Hold right mouse for the cockpit cursor. Click **FLY HELICOPTER**, press N, or select **Fly Helicopter** through X then A to return. Successful 60-meter taser shots preserve health and immobilize living suspects for retrieval, handcuffing and police delivery. Recharge takes 1.25 seconds. The former police passenger view has been removed.

Each helicopter discharge leaves two continuous white electrical trails for 0.3 seconds and plays the original laser cue. Successful disables add a separate impact cue; blocked shots and misses do not. Vehicle hits also animate electricity around the car's oriented body for 0.7 seconds. The car remains disabled for 16 seconds.

Pedestrian contacts also produce 0.7 seconds of animated white electrical arcs. The arcs follow the person's position, rotation and currently visible standing or fallen pose. Repeated hits and civilian contacts receive cosmetic feedback while retaining existing health and arrest rules. Hidden or removed targets clear the body effect.

Either Shift key makes the grounded pilot run at twice walking speed with the original run animation and faster footsteps. Releasing Shift returns to walking; the superpower cheat retains its existing dog speed. Normal helicopter travel is also 30% faster, with the existing Shift flight boost applied on top.

Arsonists receive ten seconds of protection from fire contact after lighting a fire. They choose a clear exit through nearby ground and avoid moving deeper into flames or entering another fire. The remaining protection time is saved, and expires normally after loading.

The initial Islandtown completion brings out three celebrating musicians. Later cities retain their original band sizes. If the preferred approach is blocked by water or buildings, the band also tries the other sides of the airport gathering point.

On pilot exit, the helicopter holds for two seconds, then lands on flat clear ground or descends into a low hover over uneven ground. A purchased harness on uneven terrain retrieves the carried patient or stunned suspect first, then the pilot; with nobody carried it retrieves the pilot directly. See [remote taser and automatic recovery](RemoteTaserAndPilotRecovery.md).

## Vehicle impacts and persistence

The first moving helicopter contact dents, tips and ignites the car, posts the original non-mission accident penalty, and sends its occupants running away. It explodes after four seconds unless extinguished first. Repeated contact cannot restart the fuse or repeat the penalty. An extinguished car can be towed, but player-caused recovery jobs do not award completion money or score. See [City tours and car accidents](CityIntrosAndAccidents.md).

Runtime saves include the helper's assignment and unlock, slung cargo identities, cable state, custody state, car impacts, boat recovery state, and pilot health. Existing save versions remain readable. Remote taser aiming ends when loading. Automatic pilot recovery reconnects to the saved helicopter, including a pilot attached to its harness. Occupied sling fittings and a helicopter carrying slung cargo cannot be sold.

## Validation

### One mission per order — October 12, 2026

The helper previously enabled automatic selection by default, including when loading
an older save. Five pre-fix assertions reproduced unsolicited assignments, saved
automatic work and the obsolete controller option. Automatic selection is now
removed from the subsystem and both Dispatch interfaces. The legacy save field is
still consumed, preserving the byte layout; explicit assignments remain valid.

Editor, Shipping and Development builds passed. The focused headless suite reported
26 successes: 25 exercised cases and one existing taser visual case that does no work
under NullRHI. Coverage includes idle/graduated helpers, both order routes, completed
and unreachable jobs, recall, invalid selections, busy delivery rejection, saved
explicit orders, legacy migration with actual loaded vehicle delivery, and a critical
patient delivered alive after recall. No interactive flight or controller check was
performed. Build logs, source comparisons and installation/backup hashes are indexed
in [the validation receipt](validation/AirSupportOrdersResults.json).

### Original air-operations implementation

The Editor and Shipping builds passed through `RebuildUnrealCpp.bat`. The final headless run passed **90 tests**: 88 clean passes and two passes with existing fixture warnings, zero failures or skipped tests. The warnings concern component mobility and a contextless synthetic world in `PlaneDeckRescue` and `SafePassengerLanding`.

The new integration test exercises both bumper press orders, menu switching and release behavior, cable isolation from menu navigation, actual taser/cage capture and cabin capacity, prisoner roof handoff, cargo save relinking, hit-local denting, car recovery and explosions, boat recovery, the helper's complete car delivery, rejection of the Base Location marker as an AI job, police taser firing, pilot health damage, and career unlock persistence. Related controller, equipment, mission, passenger, save, medical, dispatch, and career tests also passed.

The automatic-pickup/Y-controls follow-up also passed all 90 tests. Added assertions exercise Y opening and switching both menus, A confirmation without flight input, R3+B exit routing, automatic suspect and injured-civilian pickup, wall/floor separation, no pickup while falling, ordinary civilian exclusion, and the drop delay. Its evidence is under `Docs/scratchpad/carry-and-y-controls/`.

Evidence: `Docs/scratchpad/air-operations/tests-final/index.json`, `tests-verified.json`, build logs, and `source.diff` against the pre-change snapshot. This supplied repository has no Git metadata.

No interactive game session was launched. Sling visuals, new service-site appearance, controller feel, and AI routes through live cities still need an on-screen gameplay check.

## Installed build

The Shipping cook, package, and archive completed successfully. The NonUFS manifest verified 869 files and all six required original-game data directories. Existing cook/staging warnings are retained in `Docs/scratchpad/air-operations/package.log`.

Installed on 2026-10-05 into `C:\SimCopterRemake\SimCopter Remake v1.0.1\Windows`. The installer copied and hash-verified nine changed files, preserved 45 local city/save files, and verified all 21 custom radio tracks. The installed executable matches the newly built Shipping executable.

A complete backup is at `C:\SimCopterRemake\SimCopter Remake v1.0.1\Windows-before-air-operations-2026-10-05`. Exact file hashes and the installation timestamp are in `Docs/scratchpad/air-operations/installation.json`.

The automatic-pickup/Y-controls update was installed on the same day after both builds and all 90 tests passed. Packaging reused the unchanged cooked assets. Five changed files were copied and hash-verified, all 869 NonUFS files verified, 45 local city/save files preserved, and all 21 radio tracks verified. Its complete backup is `C:\SimCopterRemake\SimCopter Remake v1.0.1\Windows-before-carry-and-y-controls-2026-10-05`; installation details are in `Docs/scratchpad/carry-and-y-controls/installation.json`. This follow-up also had no interactive gameplay check.

## Warehouse and gameplay follow-up — October 11, 2026

The subsequent [hangar reference reconstruction](HangarReferenceMatch.md) replaces the original world shell and corrects the furniture scale/orientation to follow the catalog illustration, with window grids, skylights, pillars, trusses, caution signs and the rear office. That later update supersedes the initial warehouse visuals described below; the gameplay changes remain in place.

The warehouse's brown car, red tool cabinet, desk, chair, shelves, crates, airplane and helicopter now have real scenery geometry inside its existing shell. They are visible through the doorway from outside, with interior lighting and normal wall occlusion. The aircraft use the original geometry and parked rotors; these decorations do not enter the player's fleet or obstruct the entrance.

The same update includes the Shift run, faster default helicopter travel, arsonist fire protection/avoidance, white taser trails and car electricity, firing/impact sounds, first-island three-member celebration, and selected helicopter/capture cage transfer described above. Agent save version 10 appends the remaining fire protection timer and still reads versions 1–9.

Both Editor and Shipping builds succeeded through `RebuildUnrealCpp.bat`. All **53 selected headless checks** passed (52 clean, one with the existing contextless world warnings in `SimCopter.UI.HangarPurchaseParking`). Both **offscreen checks** passed: individual warehouse props remain visible through the shell under physical camera exposure, the rear wall hides them, and white trails/car arcs appear and expire correctly. Runtime tests verify firing and impact audio requests; no manual listening or gameplay check was performed.

The installed executable was replaced and hash-verified on October 11, with its prior version preserved at `Docs/scratchpad/hangar-interior/installed-backup/`. Existing cooked content was reused. The publication checkout was not changed or pushed. Build logs, screenshots, regression reports, installation hashes and the source comparison are recorded in [the validation receipt](validation/HangarGameplayUpdateResults.json).

The pedestrian-shock follow-up passed both build wrappers, all nine selected gameplay checks and the offscreen taser check, including original standing and injured pedestrian figures. Runtime checks cover movement/rotation, animation, expiry, repeated contact, target removal, civilian health, blocked shots and existing vehicle behavior. It was installed with a verified executable backup; no interactive playtest was performed. See [pedestrian shock results](validation/PedestrianTaserShockResults.json).
