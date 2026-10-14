# SimCopter Remake — Timmyty gameplay fork

Fly rescue missions, fight fires, recover vehicles and build a helicopter fleet in
the cities of *SimCopter*. This Unreal Engine 5.8 fork combines restored 1996
behavior with new tools, on-foot rescue work and modern controls.

**[Download v1.0.14 for Windows](https://github.com/timmyty/sim-copter-remake/releases/latest)** ·
[Release notes](Docs/Releases/v1.0.14.md) · [Controller guide](Docs/ControllerSupport.md) ·
[All changes](Docs/ForkChanges.md)

Built on [JamesIV4's original remake](https://github.com/JamesIV4/sim-copter-remake)
and [wyozi's fork](https://github.com/wyozi/sim-copter-remake), preserving their
history and authorship. The capture cage, police tasers, AI helper and expanded
recovery systems are deliberate additions to the remake.

## Start playing

1. Download **`SimCopterRemake-v1.0.14-Windows-x64.7z`** from the latest release.
2. Extract the **entire archive** and run **`SimCopterRemake.exe`** in the extracted folder.
3. Choose **New Career Game** or **New User Game**, then select a city.

Keep `Engine`, `SimCopter`, `SimCopterRemake` and `DriveInVideos` beside the launcher.
Unreal Editor is not required. If Windows requests the Visual C++ runtime, run
`Engine/Extras/Redist/en-us/vc_redist.x64.exe` and launch again.

## New in v1.0.14: optional game music

Add **959 recordings from 32 game collections** to the radio. Choose from 19 named
music packs (smaller collections are grouped around 100 MB) or the All-Games bundle.
Extract and run **Install-Music.cmd**, choose the game folder, then restart and
select the collection in Radio. Original audio quality
and the existing FLAC soundtrack are preserved. The complete SimEverything archive
is also available as a separate preservation download.

[Music downloads and installation](Docs/OptionalMusic.md) · [Release notes](Docs/Releases/v1.0.14.md)

## Also included from v1.0.13: smaller music and optional diagnostics

All 21 KINV songs now use compact 128 kbps stereo Ogg Vorbis, reducing their size
from 849 MiB to 72.65 MiB. The original-game lossless FLAC soundtrack is preserved.
Saved KINV positions, song order, pause and seeking remain supported.

The normal game download contains Shipping only. For GPU/performance diagnostics,
download **`SimCopterRemake-v1.0.14-Optional-Development-Windows-x64.7z`** from the same
release and extract its contents **inside the extracted game folder**, beside the
root `SimCopterRemake.exe`. This adds the matching Development executable and both
diagnostic launchers. It requires the full game download and is optional for play.
[Release notes](Docs/Releases/v1.0.14.md) · [Music details](Docs/InvokeRadio.md)

## Also included from v1.0.12: controller navigation, medical crew and cockpit improvements

Browse consecutive hangar helicopters without losing the controller highlight.
Deployed medics prioritize nearby patients and reserve seats for teammates' rescues;
when leaving a landed helicopter they take an onboard patient with them.

The full radio menu adds station/track selection, pause, seeking and volume with
controller navigation. Remote taser autopilot has a saved ON/OFF switch. Medical
missions guide you to the nearest hospital, while the minimap's Turbo lamp and
help strip show clear shared status. Saved bucket/harness displays and empty
mission-message panels are corrected, and the two longstanding automation fixture
failures have been repaired. [Release notes](Docs/Releases/v1.0.12.md) |
[Radio controls](Docs/RadioMenu.md) | [Medical crew details](Docs/memory/hangar-medic-deployment.md)

## Also included from v1.0.11: lossless music, Photo Mode and custom cities

Fifteen verified original songs now play from shared stereo FLAC masters. Custom
cities progress through four score-based stages, with equipment and passenger
limits that fit the current helicopter. Photo Mode adds a free camera, clean
screenshots and temporary lighting controls. Browse for cities or saved games in
any folder; use F1 for expanded help and Shift/L3 to toggle Turbo or boost.
[Release notes](Docs/Releases/v1.0.11.md) | [Music details](Docs/HighQualityMusic.md) |
[Photo Mode and progression](Docs/CustomCityProgressionAndPhotoMode.md)

## Also included from v1.0.10: emergency response and flight stability

Medical alerts are limited to one per minute, medics unload the most wounded
first, and dropped medics can retrieve previously ignored injured rioters.
Emergency vehicles appear less often, and dispatch activates the nearest
available street vehicle or sends one from HQ when its road journey is quicker.

Rooftop exits and posted personnel have stronger footing checks. Responder
pathfinding spreads expensive work across frames to address a reproduced source
of stutter. Remote taser aiming holds the aircraft steady, adds 1x–4x scope zoom,
and returns to flight facing the target. The AI helper now waits for an explicit
order for each mission. [Release notes](Docs/Releases/v1.0.10.md)

## Also included from v1.0.9: baseball, city life and clearer drive-in audio

[![A live Canyon baseball game with red visitors, blue defenders and a fixed stadium scoreboard](Docs/screenshots/baseball-stadium-v1.0.9.png)](Docs/screenshots/baseball-stadium-v1.0.9.png)

Nearby baseball fields now host complete nine-inning games with base running,
counts, scoring and a fixed stadium scoreboard. Only active players are spawned,
with at most two nearby games. [Baseball details](Docs/BaseballGames.md)

This release also restores ambient reactions, improves rooftop arrests and police
medical retrieval, corrects burglar braking and airport headings, restores
megaphone reactions, and accepts properly placed boat drops at harbors or police
roofs. Drive-in music now honors Music Volume, Game Volume and Auto-Quiet: HSI is
about 6.35 dB stronger at 135% music with full game volume and Auto-Quiet off.
[Release notes](Docs/Releases/v1.0.9.md)

The optional `Launch-With-Performance-Diagnostics.cmd` records timing traces and
GPU crash evidence together. The reported witness-photo slowdown and GPU compute
fault remain under investigation. [Diagnostics](Docs/WitnessPhotoStutterInvestigation.md)

## A rebuilt hangar and a fleet that stays yours

[![Revamped hangar: skylights, steel trusses, workshop, office and parked aircraft](Docs/screenshots/hangar-v1.0.8.png)](Docs/screenshots/hangar-v1.0.8.png)

Walk into the airport's rebuilt hangar, with tall gridded windows, roof trusses,
skylights, a rear office and stairs, workshop furniture and parked aircraft.
The equipment page identifies the selected helicopter and previews its fittings.
Purchases apply across your owned fleet, and every owned helicopter follows you
to the next city's airport. The victory band stays around the hangar base.
[Hangar details](Docs/HangarReferenceMatch.md) · [Fleet restoration](Docs/CareerFleetRestoration.md)

## Trap and recover a car with the capture cage

[![A helicopter's deployed capture cage holds a car above its sealed floor](Docs/screenshots/capture-cage-car-v1.0.8.png)](Docs/screenshots/capture-cage-car-v1.0.8.png)

The cage replaces the tow clamp and can recover a stalled, fleeing or taser-disabled
car, or an eligible boat hull after its passengers are rescued. Lower it over the
target: the floor seals automatically around captured cargo. Raise the cage, fly
to the appropriate recovery site, then lower and open it over the lit drop zone.
Take cars to a junkyard, boats to a harbor, or a captured criminal vehicle to a
police roof. The minimap marks the relevant destination while you carry cargo.

The cockpit flap has **LOWER**, **RAISE** and **OPEN** controls. The cage also
retains its four-person capacity; one vehicle or hull occupies it by itself.
[Capture and delivery controls](Docs/CaptureCage.md)

## Cheat Codes, explained in the pause menu

| Find the guide in the pause menu | Browse the codes and their effects |
| --- | --- |
| [![Pause menu with the Cheat Codes entry highlighted](Docs/screenshots/cheat-menu-v1.0.8.png)](Docs/screenshots/cheat-menu-v1.0.8.png) | [![Cheat Codes guide showing code names, effects and navigation](Docs/screenshots/cheat-guide-v1.0.8.png)](Docs/screenshots/cheat-guide-v1.0.8.png) |

Highlight the blank plate above **Continue** to reveal **Cheat Codes**. The guide
covers 20 commands with scrolling, keyboard and controller navigation. Resume
the game and press **Ctrl + Alt + X** to enter a code. The original UFO effects,
Gort dialogue, money cheats and drive-in movie commands are included.
[Full cheat guide](Docs/CheatCodes.md)

These screenshots are direct Unreal captures of the current baseball field,
hangar, gameplay components and menu widgets. The cage scene uses a neutral demonstration floor;
it shows an actual captured car and sealed cage state.

## Also included from v1.0.8

| Area | What changed |
| --- | --- |
| **Police and tasers** | Animated handheld energy taser, visible remote discharge, full-turn remote aiming and 16-second vehicle stuns. Officers retrieve stunned suspects; restrained suspects no longer accrue escape penalties. |
| **Pilot recovery** | Automatic landing waits for the pilot to reach the ground and chooses a reachable nearby surface. A purchased harness can receive a carried patient on approach, then recover the pilot. |
| **Medical response** | Medics route across steep terrain, reserve separate patients and report an empty search. Rooftop casualties fall with demolished buildings; service crews remain available. |
| **Controllers** | X opens Tools/Dispatch, Y boards or exits, A confirms, and L3 toggles Turbo Mode. Minimap chords, contextual help and hangar menu focus are improved. |
| **Career and airport** | Fleet-wide equipment, restored parked purchases after graduation, clearer equipment selection, and a band that remains on the base. Recovery jobs no longer consume original mission slots. |
| **Rendering and diagnostics** | Clearer taser effects, corrected construction-lot surfaces and a separate GPU diagnostic launcher. The reported GPU compute fault remains under investigation. |

[Detailed release notes](Docs/Releases/v1.0.9.md) explain the changes and their
validation. For GPU crash investigation, `Launch-With-GPU-Diagnostics.cmd` starts
the separately downloaded optional Development executable with extra diagnostics; the normal launcher
continues to use Shipping. [Diagnostic guide](Docs/GpuCrashAndSprint.md)

## More to explore

- **45 maps and 30 city introductions**, plus imported `.sc2` cities, day/night
  lighting, animated turbines and the original mission types.
- **Helicopters and Transport airplanes**, visible cabin occupants, catalog paint,
  a hidden Apache encounter, parachutes and on-foot carrying/rescue.
- **An AI helper pilot**, unlocked by graduating the current city, that transports
  real passengers and cargo through the same recovery rules.
- **Living cities**, with lane-aware traffic, congestion handling, accidents,
  emergency crews, building damage and spaced victory formations.
- **Original radio stations plus KINV**, with 21 Invoke the Revoked tracks,
  retained radio settings, and two supplied drive-in movies with positional audio.

[Air operations](Docs/AirOperations.md) · [Medical gameplay](Docs/NpcMedicalAndHelicopterUpdate.md) ·
[Aircraft and destruction](Docs/AircraftAndDestruction.md) · [Radio and music](Docs/InvokeRadio.md)

## Building and validation

Use **Git LFS** when cloning: music, prepared videos, textures and Unreal assets
use LFS. Supply the original game data and the required Unreal/NVIDIA build
dependencies locally, then use `RebuildUnrealCpp.bat`.
[Build instructions](Docs/ForkBuild.md) · [File inventory](Docs/ForkFileInventory.md)

This remains an evolving remake. See the [current validation report](Docs/ForkValidation.md)
for automated results, screenshot checks and known limitations. Release testing
does not claim a manual flight, controller or listening session.

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
