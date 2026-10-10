# Original city intro routes

The level flyovers now use the **full cityride movies** as their reference. The
previous 24-second orbit followed the short career-selection previews; that was
the wrong reference for this feature.

The locally supplied original CD contains `CITY0_B.SMK` through `CITY29_B.SMK`.
All 30 are SMK2, 400x216, 451 frames, with a signed frame-delay field of `-6666`.
That means 66.66 milliseconds per frame and **30.06366 seconds** per movie. The
runtime uses this duration, without stretching it when a frame takes more than
100 ms. Switching to another application still pauses the presentation.

`FUN_0044ce50` calls `FUN_00407c50(3, cityName, ...)`, which appends `_b.smk`,
then opens it through `FUN_00448400` / `SmackOpen`. Mode 2 appends `_s.smk` for
the separate 200x108, 75-frame selection preview. The reference movies and their
copyrighted frames remain local and are not included in source publication.

## Camera adaptation

`SimCopterCityIntroRoutes.cpp` contains a script for each career city. These are
hand-authored adaptations of the inspected movies, **not recovered numeric camera
tracks**. They retain the shot order, descent, low views, subject passes and
landmark endings while using the remake's actual tile positions and heights.
Continuous cubic interpolation joins the timed positions and look targets.
Camera clearance follows rendered buildings and loaded terrain; nearby required
lifts are eased to avoid sudden jumps. Window fitting applies to the establishing
view, rather than forcing the whole map into every close view.

City16 (Roseland) keeps its wide opening, island approach, waterfront building
pass, downward sweep over water and airport ending. Denser frame inspection
located the close airplane at approximately **20–21 seconds**. The aircraft passes
close to the camera there; the camera does not switch to a separate opening chase.

City18 (Myrtle Dam) descends past the dam and airport, crosses the forest and ends
on the wind farm with the city behind it, moving closer to individual turbines
in the final seconds. The apparent radar dishes are the original
`PP200` / XBLD `0xc8` wind-power plants. At the user's request, their orange circular
movie appearance is deliberately replaced by pale, lit, three-blade turbines.
The original tower and nacelle remain; rotor hub positions and swept radii come
from the original disc vertices. Blades turn at 12 RPM with staggered phases,
both in gameplay and during the paused intro. Rotors belong to their building,
so demolition and restored demolition saves remove the matching blades.
The turbine replacement needs no new material asset, orange tint or glowing disc.

Aircraft use original airplane and helicopter meshes, with an animated rotor on
the helicopter. Cinematic aircraft reuse the flyable fleet's shade-ramp paint
conversion; type-11 propeller/rotor blur is translucent rather than a solid
red/orange polygon. This fixes the close-up silhouette without changing the
normal ambient fleet. Calebopolis follows one helicopter around its mountain, with no added airplane;
its orbit clears the rebuilt terrain and rooftops. Meanwhile,
Circlopolis, Four Cities and Metropolis include helicopter passes. Presentation
aircraft never collide or advance the saved ambient fleet. Mission time remains
paused; the camera, HUD, input and the intro's own pause reference restore afterward.

Imported cities have no original cityride movie. They receive a terrain-adapted
overview, descent and airport/landmark finish with the same duration.

## Original flyover music

Each career city's tour plays the audio embedded in its matching `CITY<n>_B.SMK`.
The CD contains twelve distinct recordings shared across the thirty cities;
City16 and Myrtle Dam have different tracks. `FUN_0044ce50` applies the original
master effects volume (`DAT_00519cc0`) to the movie player. The remake follows
that control, independently of the cockpit radio slider.

Run `python Tools/Unreal/BakeCityIntroAudio.py --source <CD-SMK-directory>` to
decode the supplied movies into `Reference/SimCopterOriginalGame/sound/cityride`.
This preserves the 11,025 Hz stereo samples and timing, without resampling,
normalizing or looping. Each track has 331,450 sample frames (30.063492 seconds),
within two samples of the camera's 30.06366-second duration. A generated manifest
records the source movie and decoded PCM hash. Original media remain gitignored.

The tour owns one finite audio component, starts it after camera setup, pauses it
when the application loses focus, and destroys it on skip, natural completion or
level teardown. Radio playback is suspended during the tour without changing
power or tuning; sequential stations retain their saved position, while shuffled
stations resume with their next item, as when re-entering the cockpit. Imported
cities reuse city0's original soundtrack because they have no CD movie.

Game builds require all thirty WAVs and stage them through the existing original
`sound` directory pipeline as loose NonUFS files in `SimCopter/sound/cityride`.
The new audio tests validate every decoded file, ownership and cleanup, radio
suspension, and actual paused-world playback through a private Windows mixer
whose output is silenced before reaching the speakers.

## Reference review and validation

All 30 movies were inspected through contact sheets, with a denser City16 review.
Local evidence is in `Docs/scratchpad/original-city-intros/`: extracted movie
headers, source hashes, contact sheets, map/model inspection, build logs,
automation reports and rendered routes. The rendering fixture uses actual city
geometry under a fixed sun, fill light and exposure; it is not a capture of the
complete gameplay sky/HUD. Aircraft-only normal captures isolate moving meshes from water and
lighting changes. Landmark checks include the full Myrtle Dam field, opaque gameplay materials,
visible blade movement in isolated render captures and demolition/rebuild behavior. No interactive gameplay check has been performed.

The final build and test results are recorded in `Docs/validation/OriginalCityIntroResults.json`.

## Replaying Myrtle Dam in the original game

In career mode, use `Warp me to career: 16`, then `Been there, done that`.
Return to the hangar, advance to city selection and choose Myrtle Dam. The
original branching table (`FUN_00408370`) gives both Happyland (city15) and
Roseland (city16) a successor of city18 / Myrtle Dam. Career progression is not
simply the next numeric map file. The cheat dialog opens with Ctrl+Alt+X.

## Final validation — October 10, 2026

Editor and Shipping builds succeeded. The broad regression run passed 136 tests
with six warning-bearing cases; after final camera/material and fixture cleanup,
all 35 focused tests passed without warnings, including all 30 rendered routes.
The final City16, Myrtle Dam and Calebopolis frames were visually inspected.
The installed executable and symbols were replaced with SHA-256 verification and
a rollback backup. Original media, saves, cities and radio files were not edited.
No interactive gameplay, live HUD/sky or listening check was performed.

## Flyover audio validation - October 10, 2026

Editor and Shipping builds succeeded. Seventeen headless tests and the actual
Windows mixer test passed without warnings. All thirty soundtrack files were
verified, and measured output stopped on focus pause, skip, completion and level
teardown, then returned on resume/restart. Unreal staged all thirty WAVs as
NonUFS files. The installed game and music were updated from that staging output
with a rollback backup and SHA-256 verification. No interactive playtest or
human listening check was performed. See [audio results](validation/CityIntroAudioResults.json).
