# Optional game music

The v1.0.14 release offers **959 recordings across 32 game collections**, about
36 hours of music. Choose from **19 named music packs** (five combined packs and 14 larger single-game
collections), or download the **Music-All-Games** bundle
from the [release](https://github.com/timmyty/sim-copter-remake/releases/tag/v1.0.14).
Music is optional; the normal game keeps its existing soundtrack and FLAC masters.

## Install

1. Install the full **SimCopter Remake v1.0.14 or newer** game.
2. Download a music pack ZIP, or all three numbered `Music-All-Games.7z` parts. Keep all bundle
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
Collections do not insert original radio commercials or DJs. Long game titles wrap
in a wider station column, with full titles available as tooltips.

## Optional music downloads

The full game is the only required download. These packs add music to it.
Choose the packs you want **or** the complete All-Games bundle; their music files
are identical. Already-installed matching tracks are skipped.

### All 32 game collections in one bundle

**959 recordings; 4.56 GB total. Download all three parts:**

[Part 1 (1.89 GB)](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-All-Games.7z.001) + [Part 2 (1.89 GB)](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-All-Games.7z.002) + [Part 3 (784.3 MB)](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-All-Games.7z.003)

Keep the parts together and extract `.001` with 7-Zip, then run **Install-Music.cmd**.

### Smaller collections, combined into five packs

Each ZIP includes its installer and keeps the listed games in separate Radio collections. Sizes use decimal MB.

| Included game collections | Recordings | Download |
| --- | ---: | --- |
| SimCity 3000 Unlimited; SimCity 4 Rush Hour | 26 | [137.7 MB ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCity-3000-Unlimited-and-SimCity-4-Rush-Hour.zip) |
| SimCity Creator DS; SimCity Creator Wii | 70 | [114.2 MB ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCity-Creator-DS-and-Wii.zip) |
| SimCity Jr; SimCity BuildIt; SimCity Social; SimCity Societies Destinations | 25 | [97.2 MB ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCity-Jr-BuildIt-Social-and-Societies-Destinations.zip) |
| SimFarm; SimIsle; SimLife; SimHealth; SimPark; SimSafari | 48 | [98.7 MB ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimFarm-SimIsle-SimLife-SimHealth-SimPark-SimSafari.zip) |
| Theme Park; Sid Meier's SimGolf; SimTower; SimTown | 38 | [116.4 MB ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-Theme-Park-SimGolf-SimTower-SimTown.zip) |

### Larger collections, one game per download

| Game collection | Recordings | Download |
| --- | ---: | --- |
| Sim Theme Park | 5 | [347.5 MB ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-Sim-Theme-Park.zip) |
| SimAnt | 89 | [222.5 MB ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimAnt.zip) |
| SimCity | 93 | [478.0 MB ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCity.zip) |
| SimCity 2000 | 246 | [824.5 MB ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCity-2000.zip) |
| SimCity 3000 | 18 | [115.9 MB ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCity-3000.zip) |
| SimCity 4 | 34 | [276.1 MB ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCity-4.zip) |
| SimCity 64 | 17 | [217.1 MB ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCity-64.zip) |
| SimCity DS | 32 | [228.1 MB ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCity-DS.zip) |
| SimCity Societies | 34 | [189.1 MB ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCity-Societies.zip) |
| SimCoaster | 9 | [267.0 MB ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimCoaster.zip) |
| SimEarth | 54 | [194.7 MB ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimEarth.zip) |
| SimMars | 22 | [105.9 MB ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimMars.zip) |
| SimTunes | 64 | [285.9 MB ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-SimTunes.zip) |
| Streets of SimCity | 35 | [269.4 MB ZIP](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimCopterRemake-v1.0.14-Music-Streets-of-SimCity.zip) |

## Optional preservation: complete original SimEverything archive

This is the untouched [SimEverything soundtrack collection](https://archive.org/details/SimEverything):
**854 original MP3s plus its cover image**, preserving music, radio commercials,
news, original filenames and artwork. It is a separate **2.58 GB archival download**.

**Download both parts:** [Part 1 of 2](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimEverything-Original-Preservation.7z.001) + [Part 2 of 2](https://github.com/timmyty/sim-copter-remake/releases/download/v1.0.14/SimEverything-Original-Preservation.7z.002).

Keep both parts together and extract `.001` with 7-Zip. Joining them reproduces
the supplied original archive byte for byte. This preservation archive has **no
installer**. For automatic installation into SimCopter, use the music packs or
All-Games bundle above, which already contain the selected additional SimEverything music.

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
Three radio-menu cases also passed with all collections loaded; offscreen 4:3 and
widescreen renders were inspected for long game names. These are automated checks;
interactive playback and manual listening were not performed.

See [catalog](OptionalMusicCatalog.json), [validation receipt](validation/OptionalMusicResults.json)
and [release notes](Releases/v1.0.14.md). `Tools/Unreal/PrepareOptionalMusic.py` stages
reviewed sources; `PackageOptionalMusic.py` creates and verifies game ZIPs and the
multipart bundle. Run `GroupOptionalMusic.py <original-assets> <new-output>` afterward
to consolidate the 18 smaller collections into five named packs and generate the
32-game/19-pack download catalog. Publish the new ZIPs and catalog alongside the
unchanged large ZIPs and multipart bundles. Keep the original assets directory as
a local backup. Source downloads and audit working files stay in the ignored
`Docs/scratchpad/optional-music` directory; audio is distributed as release assets.

Download regrouping was separately verified: every one of the 1,005 music payload
files matches the original bundle, and all five grouped installers passed fresh
installation and repeated-installation checks. See [packaging receipt](validation/OptionalMusicDownloadGroups.json).
