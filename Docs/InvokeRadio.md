# Invoke the Revoked radio — October 5, 2026

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

## Song sources

Measured all 372 audio files under `D:/AICreating/MusicCreation`. Selected the most
recent complete mix within 0.75 seconds of each screenshot duration; excluded
stems and consolidated instrument recordings. All 21 imports meet that tolerance.
Converted to stereo 44.1 kHz signed 16-bit PCM because the game's loose WAV decoder
requires PCM. No trimming, padding, or time stretching was used.

Source filenames, modification times, measured durations, playback order, and imported
SHA-256 hashes are recorded in [InvokeRadioTracks.json](InvokeRadioTracks.json).

Two source choices deserve review: “Disconnect the Net” uses the 176.293-second
export rather than the newer 177.21-second master, to match 2:56. “I Didn't Build
This Life Alone” uses `PreviousNotUsing/Growth Struggle (Remix).wav`, the only
available complete audio mix matching 4:01 (240.840 seconds). Other Growth Struggle
mixes are 3:48. “Dive to Hell” is available only as a complete WAV named
`Dive to Hell for Super Earth (previous).wav`, matching 3:40.

## Implementation and validation

Authored music resides at `SimCopterRemake/Content/Radio/InvokeTheRevoked`, staged
as loose NonUFS content through `Config/DefaultGame.ini`, so it works with both
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
