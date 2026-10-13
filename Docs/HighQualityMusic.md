# Lossless original soundtrack

The Windows release replaces fifteen low-quality original songs with the supplied 44.1 kHz, 16-bit stereo FLAC masters: jazz 1–2, rock 1–6, techno 1–6 and the hangar theme. Four other jazz songs, all classical music, menu/career music, city-tour audio, KINV, DJs, commercials and effects retain their existing recordings.

## Matching and quality

Each supplied recording was compared against every original radio song and the front-end/hangar music using 32 log-frequency bands and three 90-second audio segments. Every FLAC's best match was its corresponding numbered original, with a clear margin over the next candidate. Some masters have different intros, endings or longer arrangements; these are the same songs, not necessarily identical edits. The full supplied masters are preserved.

[Match measurements](validation/HighQualityMusicMatches.json) record the comparisons and source SHA-256 hashes. [The manifest](../SimCopterRemake/Config/HighQualityMusic.json) pins both original and replacement bytes and independent FFmpeg-decoded PCM hashes. Original radio songs are 11.025 kHz, 8-bit mono; the original hangar is 11.025 kHz, 8-bit stereo. The FLACs retain their supplied stereo channels and 16-bit samples without resampling or lossy recompression.

## Shared files and fallback

Fifteen files live under `SimCopter/sound/music-hq`. Genre stations and KMIX resolve their playlist entries to those same physical files. The release omits 29 verified original WAV files (14 genre tracks, their 14 mix duplicates and the hangar theme). Playlists retain all songs and the normal shuffle behavior.

The original reference WAVs remain in the developer checkout. A custom recording with a retail filename is preserved: runtime resolution compares its hash before applying an upgrade, and game staging only omits hash-matching originals. With an unavailable FLAC, an existing original WAV remains playable. A missing replacement fails the Windows game build, preventing an incomplete package. Non-Windows builds retain original WAV playback; this release validates Windows only.

The implementation links Unreal's bundled libFLAC 1.4.3, with its notice shipped in `SimCopterRemake/Config/FLAC-LICENSE.txt`. Complete-stream checks, sample-count bounds and FLAC's PCM checksum reject damaged/truncated masters. Radio FLAC decoding runs on a cancellable worker; tuning, power-off or world teardown discard the pending result. Radio music remains uncached; decoded memory belongs to the current track, so tuning through stations does not accumulate all masters in memory.

## Reproduce

From the repository root, run:

```powershell
python Tools/Unreal/PrepareHighQualityMusic.py "C:\path\to\SimCopter_soundtrack"
```

The tool checks all supplied and original hashes before copying any masters. Original game media and the FLAC recordings are distributed in the playable release, rather than added to Git source history. The build rules stage them as loose NonUFS dependencies, using the existing original-game staging remap.

The music-only change adds 415,540,162 installed bytes: 518,651,756 bytes of FLAC replace 103,111,594 bytes of original WAV copies. Final release download size also depends on compression and the other included changes.

## Validation

Automation decodes every complete master and compares all PCM bytes with independently decoded FFmpeg checksums. It also covers corrupt/truncated input, shared genre/mix paths, restored entries when packaged WAVs are absent, missing replacements, custom-recording protection, original station counts, hangar playback, radio seeking, muting and stopping. Release-wide build and regression results are recorded with the v1.0.11 release. No manual listening check is claimed.
