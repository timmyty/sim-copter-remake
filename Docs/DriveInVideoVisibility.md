# Drive-in video visibility fix

The `hsi` and `Lights, Camera, Action!` cheats could play sound while the theater
screen appeared blank. Video decoding and the material worked, but the screen's
normal pointed away from the parking lot. Its depth offsets placed the picture
behind the theater's original face and behind the added black letterbox backing.

The corrected normal and triangle winding put the backing in front of the original
screen and the movie in front of the backing. The existing movie material, media
files, aspect-ratio fitting, spatial sound and cheat behavior are retained. This
requires an updated executable; no asset recook is needed.

## Regression evidence

The earlier test drew the media material directly onto a render target. It proved
decoding but did not prove that the picture was visible on a theater in the world.
The new test loads Sea Cliff, builds its actual theater, enters both movie cheats
through the player controller (including lowercase `hsi`), and captures the screen
from its parking lot. It compares the scene with only the movie section hidden,
keeping the theater and black backing visible, so the decoded picture itself must
contribute visible pixels. Both movies are also checked for dimensions, audio
tracks, looping, switching, repeated-code stop behavior and demolition cleanup.

Before the correction, enabling either movie changed **zero** scene pixels. After
the correction, both pictures appear on the theater. The scene captures below use
controlled exposure and no sun to make the movie surface easy to inspect; they are
offscreen captures, not a manual flight or listening test.

| Before: HSI hidden | After: HSI visible |
| --- | --- |
| ![Hidden HSI picture](screenshots/drive-in-before-fix.png) | ![HSI on the actual theater](screenshots/drive-in-hsi-fixed.png) |

![Original movie on the theater](screenshots/drive-in-original-fixed.png)

Build and test results: [DriveInVideoVisibilityResults.json](validation/DriveInVideoVisibilityResults.json).
Local reproduction, build logs, test reports and installation receipts:
`Docs/scratchpad/drive-in-video-fix/`.
