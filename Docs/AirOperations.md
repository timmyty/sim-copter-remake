# Air operations, recovery and police update

Implemented 2026-10-05. These are user-requested additions to the original SimCopter behavior.

## Controls

| Action | Controller | Keyboard / mouse |
| --- | --- | --- |
| Rotate helicopter left / right | LB / RB individually | Existing flight controls |
| Open Tools | Press **Y** | Existing tool panel |
| Switch Tools / Dispatch | Press **Y**, no bumper hold required | Click the corresponding panel |
| Select and confirm | Right stick, then **A** | Click selection |
| Cancel wheel | B | Existing panel controls |
| Passenger menu | X in Tools | Click a seat portrait |
| Recall ground services and helper | X in Dispatch | Recall from Dispatch |
| Select tow clamp / capture cage | Tools wheel | T / V |
| Deploy, close cage, or grab with clamp | X | R or left click |
| Raise / lower sling | D-pad up / down | Page Up / Page Down |
| Open cage or release clamp | D-pad left | G |
| Enter / leave remote taser mode | R3 + Y | N or cockpit AIM TASER / FLY HELICOPTER button |
| Aim / fire in remote taser mode | Right stick / X or RT | Hold right mouse and move / left click |
| Aim / fire taser on foot | LT / RT | Right mouse / left mouse |
| Carry downed person | Walk up; automatic | Walk up; automatic |
| Enter helicopter on foot | Y (or walk close) | F (or walk close) |
| Exit helicopter | R3 + B | F |
| Toggle parachute while falling | B | P |
| Put carried person down | X | G |

Y opens Tools and toggles between Tools and Dispatch. The menu stays open without holding any button; A confirms and B cancels. Both bumpers together remain an optional hold-and-release shortcut. They suppress helicopter turning while the wheel is open; the remaining held bumper stays suppressed until both have been released, preventing an accidental turn. R3+B exits the aircraft; R3+Y toggles the remote taser and automatic orbit.

## Recovery

Buy a **Tow Clamp ($1,200)** or **Capture Cage ($1,800)** from the hangar equipment catalog. These are per-aircraft fittings. The sling swings behind aircraft movement, collides with scenery, and extends from 1.4 to 18 meters.

New auto repair yards and junkyards occupy suitable clear, flat ground beside roads. Harbor sites occupy clear shoreline water and contain a crane, deck, and hull support cradle. Sites are generated for the current city; inland cities have no harbor. A deployed clamp's HUD reports the nearest appropriate site's bearing and distance.

Lower the clamp near a stalled car or capsized hull and use it again to grab. Bring the cargo slowly over a compatible service platform and lower it to complete delivery. Recovered vehicles remain on the platform briefly. The city periodically creates ordinary breakdowns, and helicopter collisions can create additional tow jobs. Capsized boats remain available for hull recovery after their passengers have been rescued.

## Arrests, cages and falls

Aim the taser on foot and fire at an active criminal within 6.5 meters. A successful shot incapacitates the actual NPC. Walk within 90 cm to pick them up automatically, then carry them into a helicopter with a free seat. Injured people and recoverable bodies use the same pickup behavior. Pickup requires a clear path and similar height, and does not happen while the pilot is falling. Putting someone down leaves a 1.5-second pickup delay. Criminal occupants display handcuffs over their passenger portraits. Hover slowly near a police-station roof: an officer takes custody of one prisoner every 1.5 seconds. Arrest mission credit is awarded on delivery.

The purchased cage has its own cockpit panel immediately below the megaphone, using the same original frame, grid, button art, colors and HUD scaling. Hold **LOWER** to form the cage directly from its visible skid fittings and lower it around people. Its bottom grille seals automatically when it captures up to four NPCs; the readout shows forming, open, sealing and occupant count. Hold **RAISE** to retract, then keep holding for another 1.25 seconds to board occupants. Cabin capacity applies; excess occupants remain in the cage. **OPEN** releases everyone still inside. The empty cage stows at the top, leaving its compact fittings visible on the aircraft. Existing V / Page Down / Page Up / G and controller controls remain available.

Owning a cage on any player aircraft increases the scheduler's crime-family weight by three times, with a minimum based on the city's average category weight so early cities also offer criminal jobs. This affects the existing robber, mugger, arsonist and speeder mix; mission limits and other jobs still apply. The helper's free cage does not qualify. Selling the final owned cage restores the original weights, and all-zero/free-roam settings remain quiet. See [capture cage follow-up](CaptureCage.md).

Released NPCs use the existing falling and medical systems. Water landings create a MedEvac incident without impact damage. Dry falls of 9 meters or more inflict health damage and can create a medical incident. The pilot can also leave an airborne helicopter and suffers fall damage; an incapacitating pilot injury triggers recovery at the aircraft after five seconds.

## AI helicopter and remote taser mode

Passing a level unlocks a second helicopter and starts automatic mission response while the player remains in that city. Each new city, including a career cheat warp, requires graduation again before the helper appears. **AIR: MISSION** in Dispatch assigns the mission currently selected on the map; **AIR: AUTO** restores autonomous work. Both require the current level to be passed. Recall returns it toward base after completing any loaded passenger or cargo delivery. The helper cruises at 600 cm/s and uses real NPCs, vehicles, mission events, and service destinations. It cannot be entered or sold by the player.

Hold either **Shift** key for 30% faster horizontal helicopter travel with a steeper flight tilt. With `superpowermultiply` active, Shift retains its existing turbo speed instead. See [traffic and flight pacing](TrafficFlowAndFlightPacing.md) for the October 11 update and validation.

Every helicopter has a visible nose-mounted remote taser. Once airborne, click **AIM TASER** on its cockpit panel or press **N / R3+Y**. No officer or purchase is required. Autopilot circles the suspect while you control the weapon through the remote helmet sight. Hold right mouse and move, or use the right stick, to aim; left click, X or RT fires. Click **FLY HELICOPTER** or use the same shortcut to return to flying. Successful 60-meter taser shots preserve health and immobilize living suspects for retrieval, handcuffing and police delivery. Recharge takes 1.25 seconds. The former police passenger view has been removed.

On pilot exit, the helicopter holds for two seconds, then lands on flat clear ground or descends into a low hover over uneven ground. A purchased harness on uneven terrain retrieves the carried patient or stunned suspect first, then the pilot; with nobody carried it retrieves the pilot directly. See [remote taser and automatic recovery](RemoteTaserAndPilotRecovery.md).

## Vehicle impacts and persistence

The first moving helicopter contact dents, tips and ignites the car, posts the original non-mission accident penalty, and sends its occupants running away. It explodes after four seconds unless extinguished first. Repeated contact cannot restart the fuse or repeat the penalty. An extinguished car can be towed, but player-caused recovery jobs do not award completion money or score. See [City tours and car accidents](CityIntrosAndAccidents.md).

Runtime saves include the helper's assignment and unlock, slung cargo identities, cable state, custody state, car impacts, boat recovery state, and pilot health. Existing save versions remain readable. Remote taser aiming ends when loading. Automatic pilot recovery reconnects to the saved helicopter, including a pilot attached to its harness. Occupied sling fittings and a helicopter carrying slung cargo cannot be sold.

## Validation

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
