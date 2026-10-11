# SimCopter Remake — Timmyty gameplay fork

This customized version builds on [JamesIV4's original remake](https://github.com/JamesIV4/sim-copter-remake)
and [wyozi's fork](https://github.com/wyozi/sim-copter-remake). Their commit history and authorship are retained.

**This fork adds its own gameplay alongside restored original behavior.** Nonlethal
police tasers, an AI helper pilot, capture equipment, expanded medical response,
parachuting and revised airplane controls are deliberate additions. Airborne
police tasers are fictional gameplay, not authentic 1996 SimCopter equipment.

The **v1.0.7 Windows download** includes smarter intersection traffic, current-city
helper unlocks, normal Shift flight boost, remote tasers, automatic landing,
capture cage controls, improved medic routing and all earlier updates.

## What this fork adds

- **Consistent people and visible cabin occupants.** A person's body, hair,
  head and clothing match across walking, helicopter seating and the passenger
  portraits. Every flyable aircraft has the actual pilot appearance, and saved
  passengers retain their identity. Original speed reactions, impact flinches,
  patient expressions and voices remain active. A medic accompanying a living
  patient visibly works with gloved hands in the portrait until the patient
  leaves or dies. The original medic benefit slows deterioration; it does not
  heal patients or make them immortal. [NPC appearance and care](Docs/NpcAppearance.md).
- **Expanded rescue and medical operations.** Survivors stand on moving wrecks;
  passengers use separate clear exit positions and wait when blocked. Walk up
  to eligible patients, stunned suspects or recoverable bodies to carry them.
  Hospital ground medics accept patients from landed helicopters, your arms or
  nearby ground drops. Trauma crews reserve patients, injury-generated calls
  move to reachable outdoor ground, and ambulances recover deceased NPCs.
  [Medical operations](Docs/NpcMedicalAndHelicopterUpdate.md).
- **Police tools, cargo recovery and helper aircraft.** Use a nonlethal taser
  from the shoulder aiming view or a helicopter's nose-mounted remote weapon, then
  handcuff and deliver living suspects for arrest credit. The cockpit's AIM TASER
  button starts automatic pursuit without an officer aboard. Towing clamps, a four-person capture cage,
  repair/junkyard/harbor destinations and an unlockable helper pilot expand
  recovery work. Helpers unlock after graduating the current city, fly at a reduced
  cruise speed, transport actual cargo and people, and respect safer
  pickup, landing and delivery clearances. [Controls and air operations](Docs/AirOperations.md).
- **More aircraft and revised flight presentation.** Fly Transport airplanes
  with independent pitch and throttle, spinning propellers, retractable landing
  gear and dashboard speed/gear controls. Helicopters have catalog-matched paint,
  cabin glazing, corrected equipment views and the hidden Apache encounter.
  Normal Shift adds 30% horizontal speed with steeper tilt, while cheat turbo keeps
  its existing speed. Exiting in flight lets the helicopter find a nearby safe landing
  or use a purchased rescue harness; the pilot has fall damage
  and a visible deployable parachute. The retractable cockpit wiring panel is
  restored, and career warps retain the selected aircraft.
  [Airplane controls](Docs/AircraftAndDestruction.md) and [pilot equipment](Docs/RescuePilotFixes.md).
- **City flyovers and a more active city.** Thirty city-specific introductions
  adapt the original full-length tours, with their soundtracks, close aircraft
  passes, animated wind turbines and skippable playback. Traffic chooses less
  congested intersection exits and can briefly phase through other stuck cars. It avoids parked
  helicopters; helicopter/car impacts can start accidents with escaping occupants
  and a short explosion fuse. Fire announcements are limited during bursts, and
  victory bands use larger, spaced formations. Apache weapons damage vehicles,
  injure pedestrians and can collapse complete building footprints.
  [Flyover details](Docs/OriginalCityIntroRoutes.md) and [collision/damage rules](Docs/AircraftAndDestruction.md).
- **Cheats, movies and music.** Original cheat entry and effects return with a
  20-code pause-menu guide, synchronized Gort dialogue and restored UFO effects.
  Drive-ins on all 45 bundled maps show the enlarged original movie or HSI, with
  positional audio at each theater and minimap cinema logos. The KINV station adds
  21 Invoke the Revoked tracks; station choice, seek position and volume persist.
  Fresh radio preferences use Rock/KROC. [Cheat guide](Docs/CheatCodes.md),
  [drive-in audio](Docs/DriveInLocalizedAudio.md) and [KINV](Docs/InvokeRadio.md).
- **Everyday improvements.** Automatic pause on focus loss, Continue-first menus,
  hangar-based level advancement, clearer controller menus, corrected witness
  photographs, a low-fuel warning, quieter emergency sirens and compatible save
  migrations support the expanded gameplay. [Complete changes](Docs/ForkChanges.md).

The flyovers are authored adaptations of the original footage, not recovered
camera tracks. This remains an evolving remake: the [validation report](Docs/ForkValidation.md)
records automated coverage, known failures and limits; no manual flight or
listening test is claimed for v1.0.7.

- [All changes, departures from the original, and credits](Docs/ForkChanges.md)
- [Complete file inventory and inherited commits](Docs/ForkFileInventory.md)
- [Build and dependency instructions](Docs/ForkBuild.md)
- [Current validation results and known limitations](Docs/ForkValidation.md)
- [Gameplay controls and air operations](Docs/AirOperations.md)

To build from source, use Git LFS when cloning: custom music, prepared MP4 videos, textures, Unreal assets and
the Windows icon are included through LFS. Supply your original game data and the required Unreal/NVIDIA
build dependencies locally. To play, use the packaged Windows download below.

## Launching a Windows build

1. Download **`SimCopterRemake-v1.0.7-Windows-x64.zip`** from the [latest release](https://github.com/timmyty/sim-copter-remake/releases/latest), extract the entire ZIP, and double-click **`SimCopterRemake.exe`** in the extracted folder. No Unreal Editor or source build is needed.
2. Keep `Engine`, `SimCopter`, `SimCopterRemake`, and `DriveInVideos` beside the launcher.
3. Choose **New Career Game** or **New User Game**, then select a city to begin.

If Windows reports a missing Visual C++ runtime, run the included `Engine/Extras/Redist/en-us/vc_redist.x64.exe`, then launch again.

## Original upstream project overview

The following describes the upstream remake's foundation. Its fidelity goals describe
the original project; the deliberate changes in this fork are listed above.

**SimCopter Remake** is a ground-up reimplementation of Maxis's 1996 PC game _SimCopter_, built in Unreal Engine 5.

The goal is to recreate the original game as faithfully as possible while making it run properly on modern PCs. It retains the original missions, helicopters, flight behavior, city importing, radio stations, and assorted Sim weirdness, but adds modern rendering, widescreen support, longer view distances, improved controls, and native support for current versions of Windows.

Several _SimCity 2000_ cities, including Cape Wells, Tokyo, and Rio, are included so you can start flying right away. You can also import your own `.sc2` cities and fly around them just as you could in the original game.

## Screenshots

|                    Flight & City Exploration                     |                      Firefighting Operations                      |
| :--------------------------------------------------------------: | :---------------------------------------------------------------: |
| [![City Flight](Docs/screenshots/1.png)](Docs/screenshots/1.png) | [![Firefighting](Docs/screenshots/2.png)](Docs/screenshots/2.png) |

|                     Maritime Rescue Operations                     |                        Winch & Passenger Pickup                         |
| :----------------------------------------------------------------: | :---------------------------------------------------------------------: |
| [![Sunset Rescue](Docs/screenshots/3.png)](Docs/screenshots/3.png) | [![Rescuing Survivors](Docs/screenshots/4.png)](Docs/screenshots/4.png) |

|                        Cockpit Camera View                         |                   Night Flight & City Skyline                   |
| :----------------------------------------------------------------: | :-------------------------------------------------------------: |
| [![Rescue Camera](Docs/screenshots/5.png)](Docs/screenshots/5.png) | [![Night City](Docs/screenshots/6.png)](Docs/screenshots/6.png) |

## Recreating the Original Game

A lot of the work on SimCopter Remake has gone into reproducing how the original game actually behaved rather than simply making a new helicopter game that looks similar.

- **SimCity 2000 cities:** Play the original 30 career cities, load included sandbox cities such as Cape Wells, Egypt Falls, Tokyo, and Rio, or import your own `.sc2` files from _SimCity 2000_. Terrain, water, roads, bridges, and buildings are converted into a flyable 3D city.

- **Original helicopter fleet:** The full helicopter progression returns, starting with the Schweizer 300 and continuing through the Bell 206 JetRanger, MD 500, MBB Bo 105, Eurocopter AS365 Dauphin, and Boeing CH-47 Chinook. Flight and landing behavior are based on the parameters and rules used by the original game.

- **Missions and career progression:** Emergency calls appear throughout the city and completing them earns money and points. Missions include transporting passengers, evacuating medical patients, fighting fires, rescuing people from rooftops and the water, clearing traffic jams, breaking up riots, and helping police apprehend criminals.

- **SimCopter's strange little world:** The remake also keeps the less serious parts of the game: blocky Sims waving frantically from rooftops, pedestrians ending up where they probably shouldn't, and the occasionally questionable logic of 1990s Sim AI.

- **Cockpit equipment:** The searchlight, megaphone, water bucket, rescue harness, water cannon, and tear gas all return, along with the familiar radar, fuel gauge, altimeter, speedometer, and passenger display.

- **Radio stations:** The original Rock, Classical, Jazz, Techno, and Mix stations are supported, including the music, DJs, and wonderfully odd commercials that played while you flew around the city.

## Modern Improvements

The underlying game is intentionally kept close to the original, but a number of things have been updated where the limitations of a 1996 PC game don't need to be preserved.

- **Modern rendering:** Cities now have a dynamic day/night cycle, nighttime lighting, shadows, reflections, and modern indirect lighting through Unreal Engine 5.8.

- **Higher resolutions and frame rates:** The game supports modern resolutions including 1080p, 1440p, 4K, and ultrawide displays, with frame rates no longer tied to the limitations of the original game.

- **Keyboard, mouse, and controller support:** Controls have been adapted for modern keyboard/mouse setups as well as Xbox, PlayStation, and other dual-stick controllers, with configurable flight inputs.

- **Native modern Windows support:** The remake is a 64-bit application designed to run directly on current Windows systems rather than relying on compatibility modes, wrappers, or emulation.

- **Longer view distance:** The extremely aggressive distance fog of the original game is no longer necessary. You can climb above the city and actually see much more of it beneath you.

## Cheat codes and drive-in videos

While playing a city, press **Ctrl + Alt + X** to open the original bitmap cheat dialog. Type a code and press **Enter** or click **OK** to apply it and resume. **Esc** or **Cancel** dismisses it without applying a code. Codes are case-sensitive. See [the full cheat guide](Docs/CheatCodes.md) for the original cheats.

Drive-in theaters play local MP4 videos, including HD and higher resolutions, with localized sound at every theater, fading with distance from each screen. The complete picture stretches across the theater screen's full width and 75% of its height, centered vertically, without cropping. Movies loop until stopped, pause with the game, and end when leaving the city or when all theaters are demolished. Cities without a theater receive one on a clear, level lot during loading. Look for the small screen/play logo on the minimap. Imported maps need a suitable 3x3 lot; occupied or uneven ground is preserved.

New profiles start on **Rock (KROC)**. Each level starts with the radio enabled on the last chosen station. A nonzero volume is retained; a previous zero-volume OFF setting returns to an audible default. See [cockpit and radio details](Docs/CockpitAndSupportSafety.md).

1. Put your videos in **`DriveInVideos` beside `SimCopterRemake.exe`** in the packaged game folder. Source builds also read `SimCopterRemake/Content/DriveInVideos`.
2. Use **H.264 video and AAC audio in `.mp4` files** for Windows playback. Use a plain filename, including spaces if needed, without a folder path or URL.
3. Enter **`Play video: My Movie.mp4`** to play that file at the city's theaters, or **`Stop video`** to stop it. Replacing a file takes effect the next time you play it.

| Cheat | Movie |
| --- | --- |
| `Lights, Camera, Action!` | Plays the restored **1440×1440** `LightsCameraActionSimCopter.mp4` at its original **10 fps**. Removing the supplied clip's fixed black border makes the actual picture **4× wider and taller** on the theater. Enter the code again to stop. |
| `HSI` | Plays `HSI.mp4`: **FuzeTheory and Invoke the Revoked - Humanist Superintelligence (HSI) - Official Music Video plays**. The supplied 1080p master was compressed to approximately **20 MB**, **1280×720 HD at 24 fps**, retaining the full video and stereo audio. Enter the code again to stop. |

The original movie restoration blends restrained Real-ESRGAN enhancement with the source, followed by light temporal cleanup and high-quality scaling. Its actual picture was only **32×32 pixels** inside a 128×128 padded frame, so this improves presentation rather than recovering native HD detail. The prepared movie is approximately **78 MB**; the source, timing and audio are preserved. See [restoration details and comparison](Docs/DriveInMovieRestoration.md). User-added HD videos play at their own resolution, and HSI retains its approximately 20 MB HD copy.

Developers can prepare files with FFmpeg and Python using `python Tools/PrepareDriveInVideos.py path/to/movie.mp4`. To target about 20 MB at 720p, use `python Tools/PrepareDriveInVideos.py path/to/HSI.mp4 --target-mb 20`. These commands preserve the inputs and write prepared copies into the source content folder. Movie playback is an MP4 extension of the original cheat, inspired by the [original drive-in format research](https://github.com/CahootsMalone/maxis-mesh-stuff/blob/master/Info/Making-Videos-for-SimCopter%27s-Drive-In-Movie-Theatres.md).

The UFO now uses the original SimCopter mesh and authored palette lights, matching the [Maxis mesh viewer reference](https://github.com/CahootsMalone/maxis-mesh-stuff/blob/master/readme-assets/mmv-gallery.png). `Radioactivity` also leaves scattered smoke and brief rubble fires after the blast. `Gort` fades in the original alien artwork and plays its timed conversation with synchronized subtitles and original left/right voices. Press **Esc**, controller **B**, or **Return to game** at any time. The full sequence returns automatically after **3:06**, with a **10-second pause** before its bonus dialogue. Like the original, the artwork is a still image rather than moving mouths.

## How It Was Made

SimCopter Remake was built through a combination of **decompilation, analysis, and reimplementation of the original game**.

Instead of trying to recreate SimCopter's behavior by eye, I analyzed the original executable and game data to understand how its systems worked. That includes things such as helicopter parameters, flight calculations, building generation, mission logic, controls, and the formats used by the game's assets and city data.

Those systems have then been reimplemented in Unreal Engine rather than simply wrapping or modifying the original executable.

That approach matters because SimCopter has a very particular feel. Some of it is intentional, some of it is probably the result of how a PC game was written in 1996, and some of it is just strange. I wanted to preserve that rather than "fixing" the game until it no longer felt like SimCopter.

The result is intended to behave like the game I remember playing in 1996, just without needing a 1996 computer to enjoy it.
