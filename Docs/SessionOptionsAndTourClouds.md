# Saved game-flow toggle and preview cloud banks

v1.0.23 follow-up: the banks now use softer, translucent fog. Career preview playback and station engine scale are documented in [the follow-up](FogEnginesAndCareerPreviews.md).

## Game selection

Every New Career, Open Career, New User and Open User selection now opens a Game Options page with a persistent Extra Remake Features toggle, Back and Continue. The retired first-run flag no longer hides the choice. The main menu returns to its original five entries. Changing the toggle remembers the choice immediately; Continue activates it before constructing the appropriate city/save picker. In-game Options changes the same preference for the next New/Open game and retains its normal OK/Cancel behavior. No application restart is required.

Saves retain their recorded gameplay mode. A mismatch returns a clear explanation before any world restoration, including how to change the toggle on the Open Game path. Existing saves are not silently converted. Career advancement retains the active career's policy.

## Tour clouds

The selection previews are the original CITY0_S through CITY29_S movies, not screenshots of the runtime sky. The earlier tour override only raised CelestialVault's distant sky coverage to 0.5, identically for every map. It could not recreate the conspicuous low cloud banks over the islands.

The new tour banks use overlapping, soft local fog volumes for thickness, depth and parallax. Each career city has its own authored bank positions and sizes, visually adapted from its preview at 2.5 seconds. These are approximations of the movie's composition, not recovered original cloud coordinates or exact reproductions. Metropolis receives no added banks, matching its clear preview. Calebopolis uses a lower bank height for its close helicopter-follow camera. The existing terrain-based tour rail stays intact. Imported maps have no original preview movie and receive a repeatable layout keyed to their city name and terrain/building fingerprint, with elevations adapted to their terrain.

Clouds drift on presentation time, pause with the tour, and are removed on skip, completion or level teardown. A separate visible actor keeps the clouds independent of the hidden GameMode that owns the tour. The previous renderer setting is restored. They have no collision, use no gameplay random numbers, create no persistent assets or save records, and apply in either gameplay mode. No additional plugin is required.

## Validation

All 611 full-suite cases passed, followed by 66 offscreen checks and the final focused regressions. All 30 career tour captures were reviewed, including Calebopolis at its lower cloud height. Editor, Shipping and Development builds, a fresh cook/package and both isolated 30-second startups passed. See [validation receipt](validation/WindowsRelease122Results.json). Original movie frames remain local reference material; repository screenshots show the remake itself. No foreground game interaction is automated.
