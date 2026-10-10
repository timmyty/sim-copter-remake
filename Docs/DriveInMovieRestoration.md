# Larger, restored original drive-in movie

The supplied `LightsCameraActionSimCopter.mp4` stores a **32x32 picture** at
`(48,48)` inside a **128x128 frame**. The rest is fixed black padding. A scan of
all **1,446 frames** confirms that later scenes also stay inside this window;
only very faint compression ringing reaches outside it.

Removing that padding makes the actual movie **four times wider and four times
taller** on the existing drive-in projection. The theater still uses its full
width and 75% height, preserving the requested framing. The entire authored movie
picture, including its internal fades and black areas, remains visible. This is
an update to the supplied media; custom videos and HSI keep their current framing.

The replacement is **1440x1440, 10 fps, 144.661 seconds**, approximately **78 MB**.
A single 4x [Real-ESRGAN restoration pass](https://github.com/xinntao/Real-ESRGAN)
is blended at 60% with a conventional upscale, then lightly cleaned temporally
and scaled to 1440p. The blend limits hard edges and invented texture. The original
frame cadence is preserved, with no interpolated frames. H.264 CRF 16 limits
additional compression damage; the AAC track is copied unchanged. The supplied
movie's audio is silent, as it was before this update.

The restored picture has cleaner edges and less visible pixel structure, but
32x32 source imagery cannot provide genuine native-HD detail. The original input
file is preserved. HSI's separate 19.8 MB, 720p prepared copy is unchanged.

## Preparation

`Tools/RestoreOriginalDriveInVideo.py` provides the reproducible preparation path.
It requires Python with numpy/Pillow, FFmpeg/ffprobe on PATH, and the official
[Real-ESRGAN ncnn Vulkan portable tool](https://github.com/xinntao/Real-ESRGAN/releases/tag/v0.2.5.0).
These are development tools; none are needed by the game.

```powershell
python Tools/RestoreOriginalDriveInVideo.py path/to/LightsCameraActionSimCopter.mp4 `
  --output SimCopterRemake/Content/DriveInVideos/LightsCameraActionSimCopter.mp4 `
  --upscaler path/to/realesrgan-ncnn-vulkan.exe `
  --work Docs/scratchpad/original-movie-restoration
```

Use a new work directory. The script checks every frame for content outside the
known crop, refuses to overwrite the source, and verifies frame count, timing,
resolution and source integrity. It writes hashes and model settings to
`restoration.json` in the work directory. Model: `realesrgan-x4plus`, one tile per
frame, 60% blend, light `hqdn3d` cleanup, Lanczos final scaling.

## Verification

| Previous prepared movie | Restored movie on the same theater |
| --- | --- |
| ![Small picture inside fixed padding](screenshots/drive-in-original-before-restoration.png) | ![Restored picture fills the projection](screenshots/drive-in-original-restored.png) |

These are offscreen scene captures from the same parking-lot viewpoint, at nearby
points in the opening scene. The physical screen dimensions are unchanged.

Results: [DriveInMovieRestorationResults.json](validation/DriveInMovieRestorationResults.json).
Editor and Shipping builds succeeded; all **five targeted tests passed**. The
playback fixture emitted its existing static-foliage attachment warnings. A full
FFmpeg decode completed without errors, and copied AAC packet hashes match the
source exactly. The packaged movie was replaced atomically with a verified backup;
the installed game executable and HSI hashes stayed unchanged. To reload an already
playing movie, enter `Stop video`, then `Lights, Camera, Action!` again.

Local build, decoder, original-source and rendering evidence is in
`Docs/scratchpad/drive-in-upscale/`. The theater playback regression checks the
1440p decoder output and requires enough visible picture area to reject the
previous embedded-border movie, while retaining the existing screen-geometry,
cheat, loop, switching and stop checks. No foreground gameplay test is performed.
