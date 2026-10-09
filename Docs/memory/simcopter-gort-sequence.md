# Timed Gort ending and return to play

Follow-up to [original assets and cheat UI](simcopter-drive-in-ufo.md), 2026-10-09.
Source and validation evidence: `Docs/scratchpad/gort-sequence/`.

## Original behavior recovered

The earlier implementation concatenated twenty WAV files over a still image. The
original instead has 72 event records (36 paired text/audio cues). The exported
Ghidra C incorrectly omits most setup records; use the disassembly of **00445800**.
`extract-timeline.py` records the stack-built structs without running the original
executable. `original-timeline.json` has all exact start times, string IDs, speakers
and one-based WAV numbers. The saved STRINGTABLE dump supplies strings **964–999**.

- Constructor **004455e0**: `martian.bmp`, 640x480; text rectangles left
  `(30,340)-(272,478)`, right `(352,340)-(610,478)`.
- Setup **00445800**: four-second delays for the main twenty lines, a **150-second
  delay** before line 21, then six-second delays for the other fifteen bonus lines.
  Main dialogue starts at 4 seconds; "Trust me..." at 80; bonus dialogue at 230;
  final subtitle at 320. Voices are NOT a simple `al01`..`al20` traversal:
  strings 977/978/979 use al15/al17/al19 with the SAME left alien.
- Update **00446af0**: clears both subtitle slots, sets the current speaker's text,
  plays its chosen WAV with DirectSound pan -10000/+10000. Record delay is relative
  to the preceding event; text/audio pairs have zero delay between them.
- Draw **00446a20**: palette fade followed by the still bitmap and light-gray
  (`0xe8`) text. **There are no mouth frames or mouth animation in this renderer.**
  Earlier notes calling missing dialogue a "mouth animation" were incorrect.
  Default font height 12 comes from **00460a70**; Slate renders the remake font.
- Original key/mouse handlers **00446ed0/00446ef0** reject early input until more
  than 16 events have run. The original waits indefinitely at the final line.

The original PE bytes were recovered from the existing saved Ghidra project:
`SimCopter.rep/idata/00/~00000000.db/db.257.gbf`, indexed XOR ChainedBuffer 10.
`recover-project-bytes.py` follows Ghidra's LocalBufferFile/ChainedBuffer layout.
Those scratch bytes were used only for static disassembly. They were never executed
or included in a build. No original EXE, new binary asset or dependency is staged.

## Implementation

- `SimCopterGortSequence` holds the original subtitle/voice pairs. Per the user’s follow-up, only the 150-second bonus delay becomes **10 seconds**, shifting the bonus start to 90 seconds and final line to 180 seconds.
- `SSimCopterGortSequence` replaces the ending branch of the cheat entry widget.
  Slate time advances with the world paused; each cue change updates the subtitle
  and starts its voice together. A late frame selects the current line without
  replaying a backlog of obsolete recordings. No separate subtitle timer drifts
  against the audio. Missing artwork/audio cannot prevent exit or completion.
- `BuildGortVoice` retains all original 22050 Hz samples, placing them in the
  speaking alien's stereo channel and zeroing the opposite channel. `PlayGortVoice`
  uses UI audio and master volume. Only one Gort voice is owned by the widget.
- Shortened bonus pause, half-second fade, text shadow, persistent **Return to game (Esc / B)** button,
  immediate cancellation and completion are remake usability changes. After six
  seconds on the final line, completion at **186 seconds (3:06)** stops its audio
  and invokes the existing `CloseScreen`. It releases the ending's pause and focus,
  restoring the existing on-foot/cockpit input mode and retaining the current pawn.
  An independent pause owner (e.g. focus loss) survives. Cancellation stops audio
  immediately, even if Slate still holds a reference to the dismissed widget.
- No save schema, mission state, aircraft ownership, media assets or city changes.

## Validation

- `build-editor-verified.log` and `build-shipping-verified.log`: succeeded. The
  Shipping wrapper confirmed the current target after another workspace build
  completed. Existing engine/toolchain warnings remain. Two initial test-only
  compilation issues (private decoder access and pointer equality assertion) were
  fixed before successful validation.
- `tests-verified/index.json`: **37 passed, zero failed/not run** across Cheats,
  Sound, Input, Controller and Polish. Two tests have warnings: existing synthetic
  city foliage attachment warnings, and the deliberate missing-WAV fixture.
  Gort tests cover all original PCM samples on both stereo channels, cue boundaries,
  ten-second bonus pause, late frames, no-audio progression, immediate Esc/B at
  several points, automatic completion, re-entry, immediate component teardown,
  unrelated-audio preservation, pause ownership, focus release, same pawn and
  correct cursor/input restoration in both cockpit and on-foot modes.
- `tests-render/index.json`: **one offscreen D3D12 render test passed**. Five actual
  Slate frames (`gort-000.00.png`, `gort-000.25.png`, `gort-004.00.png`,
  `gort-008.00.png`, `gort-180.00.png`) were visually inspected: black intro, fade,
  left/right subtitles and the longest final line all fit; the exit button remains
  visible. No foreground gameplay or live listening check was performed.
- `reference-verification.json`: all 36 original strings, speakers and voice
  numbers matched independently against the recovered events/string table; only
  the requested bonus delay differs (150 seconds becomes 10).
- Installed EXE/PDB and manifest timestamps in
  `C:\SimCopterRemake\SimCopter Remake v1.0.1\Windows`, with verified backup
  `Docs/scratchpad/gort-sequence/before-install-20261009-021005/`.
  `installation.json` and `release-files.json` record exact hashes.
  `asset-preservation.json` confirms all **21 original ending assets** match the
  source and **201 city/radio/video/save files** retain their pre-install hashes.
  This is a C++-only update using existing loose data; no recook was needed.
