# Invoke the Revoked radio — October 5, 2026

The [cockpit radio menu](RadioMenu.md) now opens with **Ctrl+R** or a double-click
on the radio. Pick an exact song or commercial; playback continues through the
displayed playlist in sequential order and loops.

Tune to **KINV**, the last station on the dial. It plays all eight Dancing to Our
Doom tracks in album order, followed by screenshot songs 1–13, then loops. No DJ,
commercials, shuffle, or deliberate four-second gaps interrupt this station.

Changing stations, powering off, or leaving the helicopter saves the current song
and position. Returning resumes the remaining PCM audio. Progress also writes
every ten seconds and at world shutdown to `Saved/InvokeRadio.ini`, and loads in
the next world/session. The filename identifies the track, avoiding dependence on
station indices. Existing stations retain their original scheduler.

Sound settings' radio slider now covers 0–200% of the master sound gain, with a
percentage readout. The cockpit radio rocker uses the same gain range. Auto-Quiet
retains its existing 0.6 multiplier: switch it off to reach the full 200% gain.
Master game volume continues to control the whole mix. The October 5 follow-up
preserves volume when tuning and saves sound settings on OK, Back, Escape, or
controller B. See [radio volume and stalled vehicles](RadioVolumeAndStalledVehicles.md).

## Compact release recordings (v1.0.13)

All 21 KINV songs now ship as stereo Ogg Vorbis at a nominal 128 kbps and 44.1 kHz.
The recordings occupy 76,176,424 bytes (72.65 MiB), down from 890,346,790 bytes
(849.10 MiB) of PCM WAVs: a 91.4% installed-size reduction. This is deliberately
lossy encoding; every song and its full arrangement remain included. The fifteen
lossless original-game FLAC masters are unchanged.

Unreal's bundled VorbisAudioDecoder module decodes on the cancellable radio worker.
Tuning, pause, seeking, mute and saved playback positions retain their existing
behavior. Legacy .wav saved filenames resolve by song name to .ogg. If an update
is extracted over old WAVs, each song appears once, preferring Ogg; old WAVs still
consume disk space until removed. A fresh extraction gives the advertised size.

Build.cs validates and stages exactly the 21 manifest-listed Ogg recordings, so
legacy WAV masters cannot accidentally return to the download. Reproduce with
`python Tools/Unreal/PrepareKinvMusic.py <verified-WAV-masters-folder>`.
The manifest in `SimCopterRemake/Config/KinvMusic.json` pins source/output hashes,
frame counts, formats and sizes. Source masters remain in release v1.0.12/history.

## Implementation and validation

Authored music resides at `SimCopterRemake/Content/Radio/InvokeTheRevoked`, staged
as verified loose NonUFS dependencies through `SimCopterRemake.Build.cs`, so it works with both
developer original-game roots and packaged installations.

Changes extend `SimCopterRadio`, `SimCopterAudioSubsystem`, `SimCopterSettings`,
and `SSimCopterSoundSettings`. The integration test exercises the actual station
and WAV playback, switch/resume offset, shortened remaining audio, ordered album
transition, playlist wrap, power-off persistence, mute, and 2x component gain.

Editor and Shipping wrapper builds passed. All 27 Radio, Sound, and Settings
automation tests passed with zero warnings/failures. The first test run found a
config-cache saving issue; direct FConfigFile persistence fixed it and the repeated
suite passed. Final label/tooltip changes were compiled in both targets.

Updated the existing playable installation with the Shipping EXE/PDB and 21
loose WAVs; no cooked assets changed, so no asset recook was needed. Installer
verifies all 23 files by SHA-256 and updates the loose-file manifest. Old binaries
and manifest are retained under `Docs/scratchpad/before-radio-2026-10-05`.
Installation evidence is `Docs/scratchpad/radio-installation.json`.

Not listened to or visually verified in a live game, following AGENTS.md section 7.

## Public fork

All 21 imported PCM tracks are included in Git LFS with the owner's explicit authorization on October 8, 2026. The public manifest retains filenames, durations and hashes; workstation source paths are kept only in the local publication evidence.
