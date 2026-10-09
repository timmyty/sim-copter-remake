# Drive-in picture sizing

Movies stretch across the theater screen's full width and occupy **75% of its
height**, centered vertically, as requested. The complete source frame is shown;
the movie is not cropped. On the square theater screen this creates a 4:3 picture
area, leaving 12.5% of the screen height above and below it.

This replaces aspect-ratio fitting. HSI's widescreen picture is one-third taller
than the previous letterboxed presentation. Square or portrait movies stretch into
the same area. Borders already encoded in custom movies remain part of those movies.
The supplied original movie subsequently received a [restoration](DriveInMovieRestoration.md)
that removes its fixed outer padding, making its actual picture four times wider
and taller within this same projection area.
Playback, spatial audio, looping, pause, cheat toggles and the corrected front-facing
screen depth order are unchanged. No media conversion or asset recook is needed.

The existing real-theater regression enters `hsi` and `Lights, Camera, Action!`,
checks the resulting picture's width, height, vertical centering and complete UV
coverage, and verifies that decoded video pixels are visible in the rendered city.
The same run covers all-map theater placement, minimap logos and movie commands.

![HSI using the requested picture area](screenshots/drive-in-hsi-three-quarter.png)

Build/test results: [DriveInPictureSizingResults.json](validation/DriveInPictureSizingResults.json).
Local evidence: `Docs/scratchpad/drive-in-full-screen/`. Captures use controlled
offscreen rendering; no manual flight or live listening test was performed.
