# Cockpit radio menu

Double-click the radio housing or its tuning strip, or press **Ctrl+R**, while
seated in the helicopter, or hold **R3** and press **View / Touchpad** on the controller.
The menu pauses flight while the radio continues playing, and uses a metal faceplate,
beveled frame, screw heads, dark inset displays and amber lettering to match the
cockpit instruments.

- Choose a station on the left and a numbered item on the right.
- **Play Track**, Enter, or double-clicking a track starts that exact item at its
  beginning and keeps the menu open. **Tune Station** starts its first item.
- The open panel has **Play/Pause**, **Previous/Next**, **-10s/+10s**, a draggable
  seek bar, elapsed/total time and volume. The closed cockpit radio keeps its existing controls.
  Seeking retains pause, clamps to the track, and works with WAV and asynchronous FLAC playback.
  Pause remains in effect when the panel closes; selecting another track resumes playback.
- Controller **A** plays the selected row, **Y** pauses/resumes, **LB/RB** selects
  the previous/next track, **LT/RT** rewinds/advances ten seconds, and **B / View** closes.
  D-pad navigates the panel. Keyboard **Space** pauses/resumes.
- Playback continues in the displayed order and loops back to the first item.
  Original stations list songs, shared commercials, DJ clips, then jingles.
  Each group is sorted by its media filename. KINV retains its 21-song album order.
- Esc, **Close**, or Ctrl+R closes browsing and returns control to the helicopter.
  Use Tab and arrow keys or the mouse to navigate; lists scroll and full track
  titles are available in tooltips.

Choosing an item powers the radio on while retaining its volume, including mute.
An explicit playlist includes its commercials/DJ clips even if the automatic
radio's Sound settings disable those categories; the stored settings are not
changed. Dial tuning to a different station returns to the existing automatic
scheduler. KINV still saves its song and playback position when leaving the
aircraft or switching away; an explicit song selection resets that song to zero.
The new original-station manual queue lasts for the current station/world.

The existing single-click tuner, right-click power control and volume rocker
remain available. The new menu is cockpit-only, cannot cover another menu,
Photo Mode or replay, and cannot interrupt a city introduction. No new assets,
plugins, cooked maps or save-format changes are required.

Implementation: `SimCopterRadio` owns discovered playlist entries and sequential
playback; `SSimCopterRadioMenu` displays that same ordering; the player controller
uses the existing settings screen's pause and input lifecycle. This is an explicit
remake extension; the cited retail shuffle and probability scheduler remains the
default until a manual playlist is chosen.

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
