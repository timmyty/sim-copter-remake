# Optional game music

The v1.0.14 release offers **959 recordings across 32 game collections**, about
36 hours of music. Download individual game ZIPs or the **Music-All-Games** bundle
from the [release](https://github.com/timmyty/sim-copter-remake/releases/tag/v1.0.14).
Music is optional; the normal game keeps its existing soundtrack and FLAC masters.

## Install

1. Install the full **SimCopter Remake v1.0.14 or newer** game.
2. Download a game ZIP, or all three numbered `Music-All-Games.7z` parts. Keep all bundle
   parts in the same folder and open `.7z.001` with 7-Zip. Extract completely.
3. Double-click **Install-Music.cmd** and select the game folder containing the
   root `SimCopterRemake.exe`. A successful install reports its destination.
4. Restart the game, open **Radio** with **Ctrl+R**, and choose a game collection
   from the station list. All collection tracks play in the displayed order.

The installer checks the complete package before copying, verifies SHA-256 hashes,
skips files already installed, and refuses to overwrite a different existing file.
It changes only `SimCopterRemake/Content/Radio/OptionalMusic/<game>`; saves and the
existing soundtrack stay intact. Install to a writable game folder; administrator
rights are not required. A cancelled folder selection installs nothing.

For a scripted installation:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Install-OptionalMusic.ps1 -GameDirectory "C:\Games\SimCopterRemake"
```

Manual extraction also works: merge the package's `SimCopterRemake` folder beside
the root executable. To uninstall a collection, remove its folder under
`Radio/OptionalMusic`. The game discovers nonempty collections at startup. Existing
radio controls support track selection, previous/next, pause, seeking and volume.
Collections do not insert original radio commercials or DJs.

## Downloads by game

Choose individual collections **or** the All-Games bundle; their files are identical.
Platform versions and alternate arrangements are identified in track names.

| Game collection | Recordings | Download |
| --- | ---: | --- |
| Sid Meier's SimGolf | 4 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-Sid-Meier-s-SimGolf.zip) |
| Sim Theme Park | 5 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-Sim-Theme-Park.zip) |
| SimAnt | 89 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimAnt.zip) |
| SimCity | 93 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCity.zip) |
| SimCity 2000 | 246 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCity-2000.zip) |
| SimCity 3000 | 18 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCity-3000.zip) |
| SimCity 3000 Unlimited | 14 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCity-3000-Unlimited.zip) |
| SimCity 4 | 34 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCity-4.zip) |
| SimCity 4 Rush Hour | 12 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCity-4-Rush-Hour.zip) |
| SimCity 64 | 17 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCity-64.zip) |
| SimCity BuildIt | 3 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCity-BuildIt.zip) |
| SimCity Creator DS | 13 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCity-Creator-DS.zip) |
| SimCity Creator Wii | 57 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCity-Creator-Wii.zip) |
| SimCity DS | 32 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCity-DS.zip) |
| SimCity Jr. | 10 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCity-Jr.zip) |
| SimCity Social | 5 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCity-Social.zip) |
| SimCity Societies | 34 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCity-Societies.zip) |
| SimCity Societies Destinations | 7 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCity-Societies-Destinations.zip) |
| SimCoaster | 9 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCoaster.zip) |
| SimEarth | 54 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimEarth.zip) |
| SimFarm | 16 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimFarm.zip) |
| SimHealth | 1 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimHealth.zip) |
| SimIsle | 15 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimIsle.zip) |
| SimLife | 9 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimLife.zip) |
| SimMars | 22 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimMars.zip) |
| SimPark | 6 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimPark.zip) |
| SimSafari | 1 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimSafari.zip) |
| SimTower | 10 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimTower.zip) |
| SimTown | 3 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimTown.zip) |
| SimTunes | 64 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimTunes.zip) |
| Streets of SimCity | 35 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-Streets-of-SimCity.zip) |
| Theme Park | 21 | [ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-Theme-Park.zip) |

## SimEverything preservation download

The [SimEverything collection](https://archive.org/details/SimEverything) is also
available in its **complete, untouched original form** as
`SimEverything-Original-Preservation.7z.001` and `.002` on this release. Keep both
parts together and extract `.001` with 7-Zip. Joining the two parts reproduces the
user-supplied archive byte for byte, including all 854 MP3s and its cover image.

That original archive preserves names, artwork, commercials and news material.
For automatic in-game installation, use the game packages or All-Games bundle
above: they include SimEverything's selected music in the correct game folders,
with the installer. The original preservation archive itself has no installer.

## Sources, comparison and quality

Sources are the [Comprehensive SimCity Music Collection v3.2](https://community.simtropolis.com/forums/topic/48504-comprehensive-simcity-music-collection-version-32/),
[SimEverything](https://archive.org/details/SimEverything), the linked NES,
Mac-soundfont and SimMars archives, and the user-supplied **SimCity 64 FLAC** archive.
**SimCity 64 (Nintendo 64DD)** and **SimCity 2000 (Nintendo 64)** are separate games
and both are represented. Source credits are retained in each collection's notes
and metadata; the soundtracks remain the work of their original creators.

Fifty-two source recordings (27 from the Simtropolis SimCopter archive and 25 from
SimEverything) matched music already present in SimCopter and were excluded from
installation. Comparison used the original radio files, front-end music, existing
FLAC masters and KINV collection. Spoken radio material, logos, standalone sound
effects and instrument samples are excluded from music playlists. MIDI files are excluded from radio playlists; the three rendered SimCity
sources are retained in notes.

Conservative cross-source duplicate removal requires matching platform versions,
strong spectral agreement and comparable duration. Different arrangements, cuts,
loop lengths and uncertain matches remain labelled alternatives. This is a count
of recordings, not a claim of 959 distinct compositions. The catalog records each
inclusion, exclusion, source and matching evidence.

**No music was lossily re-encoded, resampled or reduced to mono.** Supplied audio
files retain their encoded bytes, except four repaired Ogg containers: SC2000 SNES
Title, Newspaper and Disaster, and Creator DS Open Asia Age. Their audio packets
were stream-copied and their independently decoded PCM hashes are unchanged;
untouched originals are retained under `Source Notes/Original containers`. Damage
already present in an archival source cannot be reconstructed by this repair.

Three MIDI-only original SimCity pieces also have clearly labelled offline
GeneralUser GS renditions. These are new renditions, with the original MIDIs
retained, not claims of original hardware recordings. Existing FLACs stay FLAC.
The supplied SimCity 64 collection contains 17 FLACs, all included unchanged.

The runtime uses a native MP3 decoder alongside its existing Ogg/FLAC/WAV support;
players do not need FFmpeg. Some theme-park suites last about 38 minutes and can
use several hundred MiB while the current track is decoded in the background.

## Verification and maintenance

All 959 complete recordings were decoded by both FFmpeg and the actual Unreal
audio path, with channels, sample rates and complete durations checked. All 38
focused radio/audio/settings automation cases passed (37 clean, one with expected
fixture warnings), including MP3 pause, seek, mute, cancellation and truncated-file
rejection. The installer has checks for version gating, path containment, corrupt
downloads, successful copy hashes, repeated installation and custom-file protection.
Editor, Shipping and Development builds succeeded. A complete installation verified
all 1,005 installed file hashes. Both packaged executables survived hidden 30-second
startups, and the Development log confirmed 38 radio stations (6 existing + 32 new).
These are automated checks;
interactive playback and manual listening were not performed.

See [catalog](OptionalMusicCatalog.json), [validation receipt](validation/OptionalMusicResults.json)
and [release notes](Releases/v1.0.14.md). `Tools/Unreal/PrepareOptionalMusic.py` stages
reviewed sources; `PackageOptionalMusic.py` creates and verifies game ZIPs and the
multipart bundle. Source downloads and audit working files stay in the ignored
`Docs/scratchpad/optional-music` directory; audio is distributed as release assets.
