# City tour pause label — October 14, 2026

City tours hold the simulation paused. `SSimCopterControllerOverlay::GetPauseVisibility` previously checked only `World->IsPaused()`, so it also displayed the gameplay PAUSED text and faded backdrop during the tour. The visibility binding now excludes `ASimCopterGameMode::IsCityIntroPlaying()`. The ordinary gameplay pause overlay and the tour's pause ownership, skip behavior and control restoration remain unchanged.

Editor, Shipping and Development builds succeeded using `RebuildUnrealCpp.bat`. All three existing focused automation cases passed without warnings: `SimCopter.CityIntro.RouteAndControlRestoration`, `SimCopter.CityIntro.LightingRestoration`, and `SimCopter.GameplayOptions.Integration`. These cover existing tour/gameplay behavior; the label visibility change was reviewed in source, without adding a test for this small UI change. No interactive visual check was performed.

Source baseline, build logs and test exports are in `Docs/scratchpad/city-tour-pause-label/`. The user then requested the related [Photo Mode lighting fix](photo-lighting-refresh.md) and closed the game. Both fixes were installed together in the playable copy, with backups and SHA-256 verification for both executables and symbols. The installation receipt is `Docs/scratchpad/photo-lighting-refresh/installation.json`.

Publication follow-up: [v1.0.15 and release cleanup](github-release-v1.0.15.md).
