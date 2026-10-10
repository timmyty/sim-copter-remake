# Drive-in movies, original UFO, Gort and cheat-entry art

Implemented 2026-10-09 in `C:\SimCopterRemake\sim-copter-remake-main`.
Player instructions: [README](../../README.md#cheat-codes-and-drive-in-videos) and
[cheat guide](../CheatCodes.md). Evidence: `Docs/scratchpad/drive-in-ufo/`.

## Movies and preparation

- `ASimCopterDriveInPlayer` owns one looping MediaPlayer/MediaTexture and a mesh/audio
  component at each standing theater. Derive the actual screen from CO182 object 7,
  face type 18, atlas 2/cell 1; apply the same unit conversion, city rotation and
  building placement as the original mesh. Movie references: `004477b0`, `0049ab10`,
  `0049ad10`; the original used 32x32 Smacker at 10 fps.
- Movies fit the authored square screen with black bars. Pausing also pauses media;
  demolished screens stop drawing/sounding, and leaving the world releases media.
  Audio is spatial, near the screen. Use the existing exposure compensation for the
  emissive screen under the remake's bright sun.
- **NewStyleOutput MediaTexture needs a regular Color/2D material sampler.** An
  External sampler compiled but rendered black. Both real video decoders and the
  material now pass a D3D12 offscreen readback, saved as `movie-0.png` / `movie-1.png`.
  A synthetic test world must call `InitializeActorsForPlay`: otherwise Actor
  ProcessEvent suppresses OnMediaOpened even when WMF successfully opens the file.
- `Tools/Unreal/CreateDriveInMaterial.py` creates/reuses `M_SimCopterDriveIn` and
  `T_DriveInDefault`. Reuse existing expression nodes: deleting all expressions on a
  material held by ConstructorHelpers/CDO caused a root-removal assertion during an
  earlier bake. The final bake and cook succeeded.
- Plain local filenames resolve first in package-root `DriveInVideos` (beside the
  launcher), then source `Content/DriveInVideos`. `Play video: filename.mp4` consumes
  the full line, preventing a filename such as Radioactivity.mp4 from activating
  another cheat. Reject paths/URLs. `HSI` and the original movie cheat toggle; `Stop
  video` stops. A missing file/theater reports a HUD message and retains any current
  valid movie.
- Build.cs declares only `Content/DriveInVideos/*.mp4` as NonUFS; DefaultGame.ini
  remaps it to package-root `DriveInVideos`. Do not pack these inside a pak or stage
  encoding intermediates. Verify the two exact movie entries in the NonUFS manifest.
- Inputs remain outside the repo, at `C:\SimCopterRemake\HSI.mp4` and
  `C:\SimCopterRemake\LightsCameraActionSimCopter.mp4`. The HSI source is 492,338,216
  bytes, 1920x1080/24 fps. Its prepared two-pass H.264/AAC copy is **19,774,886 bytes,
  1280x720/24 fps, 206.336 seconds**, with 96 kbps audio. Framewise SSIM against the
  source scaled to 720p is 0.8904; the requested small size loses fine detail.
- The initial original-movie copy was **3,154,014 bytes, 1080x1080/10 fps**.
  **Corrected by the [movie restoration](../DriveInMovieRestoration.md):** inspecting
  every source frame showed a fixed 32x32 picture inside the 128x128 frame. The
  earlier warning that later scenes fill the outer frame was incorrect. The new
  1440x1440 copy removes that padding and uses restrained neural restoration.
  `Tools/PrepareDriveInVideos.py --target-mb 20` still prepares HSI's compact HD
  variant; use `Tools/RestoreOriginalDriveInVideo.py` for the padded original movie.

## Original assets and aftermath

- The [Maxis mesh viewer gallery](https://github.com/CahootsMalone/maxis-mesh-stuff/blob/master/readme-assets/mmv-gallery.png)
  was reviewed. Replace the invented UFO hull with original SIM3D2.MAX object
  **0x17c**: preserve the authored antenna, center structure, rim and palette.
  Type-25 light markers feed the existing flashing-light renderer. Exclude type-11
  effect cards and type-25 markers from opaque hull geometry. `ufo-restored.png` is
  an offline Blender view of the exported exact mesh, not a gameplay screenshot.
- Nuclear `004a6940` scatters class-1 tile effects with `(rand() & 31) == 0` after
  actual building destruction. Class 1 is short-lived smoke. The user's requested
  visible fires are a documented addition: small cosmetic rubble flames at those
  locations for 30 seconds, fading over the last three, capped at 100; no extra
  mission, score or save state. NPC removal/mission termination remain unported.
- Gort originally shipped here as **martian.bmp, 640x480**, with concatenated
  22050 Hz recordings. This is superseded by the [timed Gort sequence](simcopter-gort-sequence.md):
  the original 36 subtitles, voice choices, stereo pan and pauses are now restored.
  `00446a20` confirms the original uses a fading still image, not mouth frames.

## Original cheat dialog

- `004354c0` selects `MBox.bmp`, `MBoxCht.bmp`, `MBoxl.bmp`, `button.bmp`, flags
  **0x10001** and STRINGTABLE **35**, "Enter your cheat code here:". The saved
  original string table is in
  `Docs/scratchpad/agent-sessions/2026-07-26-hangar-shell-and-flaps/strtable.txt`.
- Page: 465x353, centered in the original 640x480 space. Input: (100,100)-(370,200),
  Windows font height 18. Buttons: (194,256) and (294,256), 100x28, original OK/Cancel
  strings 20/21. The nine-frame lever occupies (52,211)-(201,304), 55 ms ping-pong.
  The shared page font uses Slate, so font rasterization differs from retail GDI.
- `00441f60` appends characters; `00442230` backspaces from the end. `004428f0`
  limits the buffer to 128 including the underscore, hence 127 typed characters.
  `00441e40` blinks the underscore every 700 ms. Ctrl+V is a convenience extension.
  There is no modern title bar or bordered edit control.
- `0044bf70` closes the page before applying the code. Enter/OK now do this;
  Escape/Cancel do nothing to game state. The HUD receives remake feedback.
  Gort acquires its own pause after the entry screen releases its pause; closing any
  settings screen also releases its retained initial-focus widget.
- The offscreen preview exposed an art cache bug: keyed/opaque GetBitmap calls
  shared one key, so the first request could leave cyan corners on the page. Cache
  them separately, including normalized filename. The test loads opaque first and
  confirms the keyed render has transparent corners. Verified images:
  `cheat-dialog-0.png` (movie code), `cheat-dialog-1.png` (long wrapped code).
- CEO instructions explicitly require entering the code, opening the hangar's
  helicopter catalog, then **pressing 1-9** to unlock/deliver an aircraft. The code
  alone grants no aircraft; the guide lists the original runtime-number mapping.

## Validation

- Editor and Shipping builds passed: `build-editor-verified.log` and
  `build-shipping-verified.log`. Existing engine deprecation warnings remain.
- `tests-verified/index.json`: **135 passed**, six of those with known synthetic
  world warnings, and one previously documented failure, `SimCopter.UI.FlapLayout`
  (old assertion expects equipment mask 127 while four original flaps cover 31).
  This same failure is recorded in [earlier validation](aircraft-passengers-road-coverage.md#validation).
  No newly introduced test failures. Do not report the whole suite as clean.
- `tests-ui-render/index.json`: **11 passed**, including actual HSI/original movie
  decoding, rendered nonblack frames, correct dimensions, audio track presence,
  loop/switch/stop behavior, original dialog and cheat effects.
- After the keyed-art fix, `tests-ui-verified/index.json`: **3 passed**, including
  transparent-corner render, keyboard/callback behavior and existing menu styling.
- Cook/package passed; `cooked-assets.csv` confirms both levels and both new media
  assets. The NonUFS manifest contains both videos plus original Gort/UI assets.
- No foreground flight playtest or live soundtrack listening was performed.
  Offscreen UI and media frames were visually inspected. Installation evidence is
  recorded separately in `Docs/scratchpad/drive-in-ufo/installation.json`.

Installed at `C:\SimCopterRemake\SimCopter Remake v1.0.1\Windows`: 14 changed
files, including the Shipping binary, cooked containers, two movies and player
instructions. All replacements were backed up before copying, then hash-verified.
Backup: `Docs/scratchpad/drive-in-ufo/before-install-20261009-013236/`.
Verified all 872 NonUFS entries and preserved hashes of 66 existing city/radio files,
including all 21 Invoke the Revoked radio tracks. No save files were overwritten.
This was a local update; no GitHub publication was requested or performed.
