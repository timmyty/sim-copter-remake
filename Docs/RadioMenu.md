# Cockpit radio menu

Double-click the cockpit radio housing or its tuning strip, or press **Ctrl+R**.
The keyboard shortcut and **R3 + View / Touchpad** also work while on foot.
The menu pauses gameplay while the radio continues playing, and uses a metal faceplate,
beveled frame, screw heads, dark inset displays and amber lettering to match the
cockpit instruments.

- Choose a station on the left and a numbered item on the right.
- **Play Track**, Enter, or double-clicking a track starts that exact item at its
  beginning and keeps the menu open. **Tune Station** tunes normally in that station's selected mode.
- The open panel has **Play/Pause**, **Previous/Next**, **-10s/+10s**, a draggable
  seek bar, elapsed/total time and volume. The closed cockpit radio keeps its existing controls.
  Seeking retains pause, clamps to the track, and works with WAV and asynchronous FLAC playback.
  Pause remains in effect when the panel closes; selecting another track resumes playback.
- Controller **A** plays the selected row, **Y** pauses/resumes, **LB/RB** selects
  the previous/next track, **LT/RT** rewinds/advances ten seconds, and **B / View** closes.
  D-pad navigates the panel. Keyboard **Space** pauses/resumes.
- Press **SHUFFLE / SEQUENTIAL** to toggle the browsed station's playback mode.
  Original stations default to the original shuffled music/DJ/commercial/jingle
  schedule, with commercials enabled by default. Optional music collections also
  default to shuffle; KINV defaults to its 21-song album order. Each station retains
  its selected mode for the current city session. Toggling preserves the current track,
  playhead, volume and pause state.
- Sequential mode follows the displayed list and wraps. The browser groups songs,
  shared commercials, DJ clips, then jingles, sorted by filename; it is a catalog,
  not the play order in Shuffle mode. Selecting a track does not change the mode.
- Esc, **Close**, or Ctrl+R closes browsing and returns control to the pilot.
  Use Tab and arrow keys or the mouse to navigate; lists scroll and full track
  titles are available in tooltips.

Choosing an item powers the radio on while retaining its volume, including mute.
An explicit track selection can play a commercial/DJ clip even if Sound settings
disable that category. Automatic shuffle playback honors those settings; sequential
mode traverses the whole catalog. KINV saves its song and position periodically,
when powered off and when switching away. An explicit song selection starts at zero.
Leaving or entering the helicopter no longer interrupts audio, decoding, seeking or pause.

The existing single-click tuner, right-click power control and volume rocker
remain available. The menu cannot cover another menu,
Photo Mode or replay, and cannot interrupt a city introduction. No new assets,
plugins, cooked maps or save-format changes are required.

Implementation: `SimCopterRadio` owns discovered playlists and playback modes;
`SSimCopterRadioMenu` displays the catalog; the player controller
uses the existing settings screen's pause and input lifecycle. This is an explicit
remake extension. The retail cycle comes from `FUN_0042e570` (one each of slots
0/1/2/3), `FUN_00430070` (category shuffle), `FUN_0042fa20`/`FUN_0042f3d0`
(initial file shuffle and wrap), and `FUN_0042f160` (four-second gap, 20% DJ,
90% commercial, 20% jingle and 10% extra music). The initial shuffle seed is the
remake's, so it reproduces the scheduling rules rather than a fixed song sequence.

Current radio/medic change and validation: [RadioAndMedicUpdate.md](RadioAndMedicUpdate.md).
The older validation records below describe their respective revisions.

Validation results: [RadioMenuResults.json](validation/RadioMenuResults.json).
Editor, Shipping and Development builds succeeded. All 49 related automation
tests passed without test warnings; the final menu checks also passed after the
keyboard-focus refinement. Both local game executables and their symbols were
installed with verified backups and SHA-256 hashes.
Build logs, source baseline, offscreen captures and test reports are retained in
`Docs/scratchpad/radio-menu/`. Interactive flight and listening are not part of
the automated checks.

## Playback controls follow-up — October 14, 2026

Editor, Shipping and Development builds succeeded. All 37 focused radio,
controller, recovery and save checks passed, including WAV/FLAC pause and seek,
pending-decode cancellation, playlist wrapping, menu lifecycle and the R3 + View
shortcut. One expected missing-file warning belongs to the existing save test.
Two additional offscreen cases passed; the radio was inspected at 4:3 and 16:9,
and the remote-taser panel at 100% and 150% HUD scale. Both installed game
executables and symbols have verified backups and SHA-256 hashes. No interactive
flight, physical-controller or listening check was performed.
See [the combined validation receipt](validation/RemoteTaserAutopilotAndRadioControlsResults.json)
and `Docs/scratchpad/remote-taser-autopilot/`. Publication checkout unchanged.

## Controller navigation correction — October 14, 2026

Use the D-pad or left stick to move the highlighted control. Press A to enter
Stations or Tracks, browse with up/down, then press A to play the selected item.
B leaves the list; press B again to close the radio. On seek and volume, A begins
adjustment, left/right changes the value, and A or B ends adjustment. Y pauses,
LB/RB changes tracks, and LT/RT skips ten seconds.

The radio now shares the other menus' controller focus outline and edit behavior.
See [investigation and validation notes](memory/saved-tools-radio-hospital.md).
