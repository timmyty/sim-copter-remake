# Localized audio at every drive-in

Every standing theater plays the current movie soundtrack from its own screen.
Sound is strongest within 300 cm and fades over the next 4,000 cm. Returning to
a theater restores its sound at the current movie time. Pausing mutes every
theater; demolishing a theater mutes only that location.

## Cause and implementation

Unreal 5.8's `FMediaPlayerFacade::ProcessAudioSamples` uses a single primary audio
sink for media players with playback timing V2. Connecting several
`UMediaSoundComponent` objects to the same player therefore creates several
speakers but supplies decoded samples to only one of them.

The movie now has one media sound decoder, downmixed to mono, which sends only
to a transient `USoundSourceBus`. Each theater has a separate spatial
`UAudioComponent` playing that bus. All screens and speakers share the movie
clock without opening extra video decoders. `PlayWhenSilent` keeps the speakers
ready when the listener returns from outside their attenuation range.

Switching or stopping movies releases the speakers, decoder and bus. The existing
screen visibility and full-width, 75%-height presentation remain covered by tests.
No engine modification, additional plugin, or new runtime dependency is needed.

## Validation

Editor and Shipping builds succeeded. All six targeted tests passed:

- `SimCopter.DriveIn.LocalizedAudio`: loads Metropolis, with eight actual theaters,
  and captures the real Windows mix near each isolated emitter. It repeats those
  measurements after switching movies and checks distance falloff, returning,
  pause/resume, demolition and stopping.
- `SimCopter.DriveIn.Playback`: executes both movie cheats and renders actual
  theater scenes; checks the requested picture framing.
- Safe placement, all 45 bundled maps, minimap logos, and movie cheat commands.

The audio test uses a private Windows mixer. Its listener captures samples after
spatial mixing, then zeros the buffer before hardware output. It does not play
the soundtrack through desktop speakers. The main editor uses
`-DeterministicAudio`; no foreground game window or manual listening is required.
The non-real-time renderer did not correctly mix this source-bus route during
investigation, so its silent results are not used as audio validation.

`LightsCameraActionSimCopter.mp4` contains a silent AAC track (FFmpeg measured
all 144.66 seconds at -91 dB). The test checks that switching to this movie does
not retain HSI audio, then switches back to HSI and checks every speaker again.
The silent source movie has not been modified.

Results: [validation JSON](validation/DriveInLocalizedAudioResults.json).
Local logs and scripts: `Docs/scratchpad/drive-in-multi-audio/`.
No manual flight or physical-speaker listening test was performed.

To run the Windows integration checks, build with `RebuildUnrealCpp.bat`, then
run `UnrealEditor-Cmd.exe` with the project path and these arguments:

```text
-unattended -nop4 -nosplash -RenderOffscreen -DeterministicAudio
-ExecCmds="Automation RunTests SimCopter.DriveIn+SimCopter.Cheats.DriveInCommands;Quit"
-TestExit="Automation Test Queue Empty"
```

Do not add `-NoSound`: the audio test needs the mixer to initialize, although its
captured output is silenced. Older full-suite results and two unrelated known
failures remain documented in [ForkValidation.md](ForkValidation.md).
