# Custom city progression and Photo Mode

Requested remake gameplay and interface changes, October 13, 2026. These intentionally extend the original game's rules.

## Custom city progression

Custom cities start with local response jobs and advance using the highest score reached in that session. Losing points does not relock an earned stage. Save version 2 stores this high-water mark; version 1 saves start from their saved score.

| Stage | Points | Newly eligible work |
| --- | ---: | --- |
| Local response | 0 | Medical, transport, traffic and suitable rooftop rescues |
| Rescue response | 500 | Boat rescues, fires, robbers and muggers |
| Advanced response | 1,500 | Train rescues, major crashes, burglars and arsonists |
| Major incidents | 3,000 | Riots |

Mission eligibility also checks the current aircraft. Rooftop, boat and train rescues need the rescue harness; fires need water equipment; crash jobs need both. Riots require an upgraded aircraft with at least three passenger seats and installed tear gas with ammunition. Buying a capture cage cannot bypass crime progression. City Settings still controls mission frequency, while difficulty follows the earned stage. Expanded controls describe the current stage and next score target.

At initial custom-city load, the mission system scans the full 128×128 tile map after geometry and traffic placement data are ready. It caches occupied buildings, roads, fire targets, tall rescue roofs and open water. Roofs must have an actual roof post at least 1.5 tiles above ground; small buildings are excluded. Boat sites need a 5×5 water neighborhood, giving them clearance from shore and rejecting small ponds and narrow canals. Roof and boat selection uses this whole-city cache without a hangar or camera radius. Demolished sites are discarded and simultaneous rescues avoid sharing a site.

## Passenger limits

New medical parties reserve one cabin seat for a medic. Other rescue and transport parties can use every passenger seat. The cap applies to career and custom games, including rooftop, boat, train and plane survivors. A two-seat helicopter therefore gets at most one medical patient or two nonmedical passengers. On foot, the current parked aircraft determines the cap. Already active or saved parties are retained.

## Controls and screenshots

- **Esc → Photo Mode** or **Ctrl+P** enters Photo Mode, pausing the simulation and hiding the HUD.
- Use **WASD**, **Space/Ctrl**, **RMB + mouse** and the **mouse wheel** to move, frame and zoom. Hold Shift for faster photo-camera movement.
- **F12** or **Take screenshot** requests a numbered `SimCopter_Photo.png` capture under the game's `Saved/Screenshots` directory. Screenshots exclude the HUD, cursor and photo panel. The panel reports the destination.
- **H** hides or shows the photo controls. **Esc** or **Ctrl+P** returns to play, restoring the prior camera, input, HUD and lighting.
- **Options → Gameplay → Day / Night** adjusts the live lighting and selects a fixed time. The photo panel also has a temporary day/night slider; leaving Photo Mode restores the gameplay clock.
- **F1** expands or collapses keyboard controls. **R3+L3** remains the controller shortcut. Bottom tool instructions are inside this expanded panel rather than always occupying the screen.
- Tap either **Shift** to toggle Sprint on foot or helicopter boost in flight. It shares the controller's L3 state. The toggle commits on release so **Shift+F2–F5** can still recall responders without changing Sprint. Possession changes and input restoration clear Sprint as before.
- After ten seconds without mouse activity, the game cursor hides. Moving or clicking the mouse restores it; leaving the game viewport restores the desktop cursor.

## Menus and custom files

Both career and custom Esc menus have a hidden **Cheat Codes** plate immediately above **Continue**. Hovering it or moving Up from Continue reveals the label and opens the existing guide. Photo Mode follows Continue. The original menu artwork supplies all the resized plates, lettering style and lever strip.

The User Game picker has **Browse...**, which opens the native Windows file dialog. A valid `.sc2` city can be loaded from any folder. The selected absolute path continues through the existing session/save system; cancelling leaves the picker intact, and invalid files show an error.

**Open User Game** and **Open Career Game** also have **Browse...**, including when their local save list is empty. These open remake `.sav` files from any folder and retain the distinction between career and user saves. Cancelling leaves the picker unchanged. New user saves preserve the original city path; when that path is unavailable, loading looks for the `.sc2` beside the save and then in the standard cities folder. Older saves continue to load. Quick Save and Save Game As retain their existing local-save behavior.

## Validation

Build, automated checks, source diff and installation evidence are recorded under `Docs/scratchpad/custom-city-progression/`. Automated coverage includes tier/gear gates, party caps, initial scans of Arcocity/La Isla/Lakeland, old and new saves, keyboard/controller Sprint, both menus, and the Photo Mode lifecycle. No interactive gameplay or native File Explorer dialog check is implied by those tests.

Editor, Shipping and Development builds succeeded. Across the broad suite and focused reruns, 503 of 505 distinct cases pass. The remaining ReferenceCity sample-fixture and FlapLayout equipment-mask failures are documented in the pre-change v1.0.10 results. The two old expectations for held Shift and three-plus boat survivors were updated and rerun successfully. Final offscreen checks passed and both menu layouts were inspected. The existing local game installation now has the new Shipping and Development executables and symbols, with hash-verified backups. No publication checkout or release was updated. See [validation and installation receipt](validation/CustomCityProgressionResults.json).

The Open User Game / Open Career Game Browse follow-up also passed Editor, Shipping and Development builds and all 21 selected save, front-end and career tests. The external-file test deliberately opens a missing save and reports one expected file-read warning. Tests cover arbitrary paths and names, matching game types, preserved state on failed opens, external city paths, adjacent city files, legacy saves and invalid files. Both Open menu previews were rendered offscreen and inspected. Both installed executables and symbols were replaced with verified backups and hashes after the user closed the game. The native Windows dialog was not manually exercised. See the [save-browser receipt](validation/OpenSaveBrowserResults.json) and `Docs/scratchpad/open-save-browser/`.
