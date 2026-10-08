# Gameplay, recovery, and UFO update

User-requested changes, 2026-10-05. These intentionally extend the original game's behavior.

## Controls, audio, and menus

- Helicopter exit follows the last left/right stick movement exceeding the 0.25 dead zone. Keyboard roll/yaw also chooses a side. Releasing the stick preserves the choice. The exit uses the selected side of the actual fuselage bounds in the aircraft's yaw frame.
- Helicopter contact immediately tilts the struck person's figure and starts its recoil clip on the same call that plays the impact voice. A fatal helicopter reaction waits for this tumble before resuming the original BHAV 912/903 death and mission accounting.
- Crash rescue siren lasts three simulation seconds, with explicit cleanup on respawn and EndPlay. Cleanup can stop the sound after the player has unpossessed the aircraft.
- A new three-pulse warning sounds once at or below 10% fuel. Refuelling above 12% rearms it. Its loose PCM file is `SimCopterRemake/Content/Audio/LowFuel.wav`; the packaged configuration stages it outside the pak for the existing audio decoder.
- Continue is the first row of the in-game menu. Original command IDs and optional City Settings remain intact.
- Application focus loss clears held controls and acquires a pause. Returning to gameplay opens the Continue menu; existing menus retain their pause ownership. This uses the existing Slate application activation callback.
- A completed career level retains the normal hangar screen and adds Next Level. That button runs the existing award and career-transfer path, guarded against duplicate awards. Enter no longer advances the city from outside the hangar. The final career city retains the existing end-of-ladder behavior.

## People and ambulances

Water/boat rescue civilians now draw from a shuffled bag of ten distinct civilian figure classes. Choosing a random legacy mesh filename did not select a different animated person; the behavior class controls the figure. The bag also spans separate one-person spawns used by aircraft rescues.

Every two seconds, the traffic system looks for deceased pedestrians, including criminals and emergency crew, that are visible and not already carried or assigned. It uses real hospital dispatch capacity and road routing. A medic walks to the body, visibly carries that same actor to the ambulance, loads it, and the vehicle drives to its hospital. The body is removed at hospital arrival. Unreachable calls or calls waiting for capacity remain pending and are retried.

Claims prevent duplicate dispatches. Aborting recovery releases the body for another crew, and a medic who dies also remains eligible for recovery. Mission cleanup and distance culling retain corpses. The traffic save blob is version 3, backward-compatible with versions 1 and 2; it now retains body identity, recovery phase, and elapsed collection time. Ground-agent carrier identity already saves and restores the loaded body.

The automatic service requires a hospital with a reachable road route. A medic cannot retrieve an inaccessible rooftop or water location merely by being dispatched; recovery times out and retries without deleting the body.

## UFO findings and replacement

The original object 380 in `geo/sim3d2.max` has no conventional painted hull texture. It contains palette-colored faces: 147 type-19 faces, 19 type-15 faces, 138 type-25 light faces, and 64 type-11 translucent faces. The previous opaque palette conversion also rendered those translucent shells solid. Thus the old appearance was partly the original low-detail geometry and partly a conversion problem; the reconstruction preview is not evidence of the exact 1996 on-screen rendering.

The replacement uses a procedural lenticular hull, raised glossy canopy, cyan perimeter ring, and lower drive ring. Flight, mission, and abduction behavior remain governed by the existing ambient vehicle system. The material assets are referenced by the ambient actor so the cooker includes them.

- [New saucer preview](scratchpad/gameplay-polish/ufo-new-preview.png)
- [Underside preview](scratchpad/gameplay-polish/ufo-underside-preview.png)
- [Previous opaque conversion reconstruction](scratchpad/gameplay-polish/ufo-old-preview.png)
- [Generated hull texture](../SimCopterRemake/Content/Art/UFO/UFO_Hull_BaseColor.png)
- [Exact generation prompt](../SimCopterRemake/Content/Art/UFO/GenerationPrompt.txt)

The hull texture was generated with the built-in image-generation tool, then saved in the project and imported through `Tools/Unreal/CreateUfoMaterials.py`. The PNG source, imported texture, and three materials are retained under `SimCopterRemake/Content/Art/UFO/`. Preview images use the exact C++ mesh exported by the geometry automation test, rendered in Blender with analogous materials; they are not live Unreal screenshots.

The generated source is 1254 pixels square. Unreal's texture build stretches it to a power-of-two image and caps the runtime texture at 1024, with mipmaps for distant viewing. Re-running the creator reuses existing material graphs: deleting expressions from materials rooted by native constructor references triggers a UE 5.8 editor assertion. The initial re-run exposed this; the corrected, idempotent importer completed successfully.

## Validation

Editor and Shipping builds succeeded through `RebuildUnrealCpp.bat`. There are existing engine deprecation warnings for `APawn::GetMovementBase`.

All 56 selected automation tests passed, with zero failures or warnings. Coverage includes the actual left/right exit positions, impact mesh tilt before ticking, queued fatal reaction, sound start and stop, 10% warning and rearming, pause ownership, five distinct rescue figures, ambulance dispatch and duplicate suppression, carrying and loading the actual body, save/restore during the return trip, hospital arrival, saucer triangle winding, and existing Settings, Dispatch, Career, Knockdown, Controller, People, and Radio tests.

The first runs exposed incomplete headless player setup and the siren cleanup's local-possession gate. The fixture now creates a local player; cleanup resolves the world audio subsystem directly. Tests and build logs are retained in `Docs/scratchpad/gameplay-polish/`, with `tests-complete/index.json` as the final result. `source.diff` records changes against the pre-task source snapshot because this supplied checkout has no Git metadata.

No live gameplay, listening, or menu visual verification was performed, following AGENTS.md section 7. Runtime walking around obstacles, presentation timing, and the UFO's appearance in city lighting still need a player's on-screen check.

## Package and installation

The final Shipping cook/package/archive succeeded. Its IoStore listing confirms the hull texture, its bulk mip data, and all three UFO materials. The NonUFS manifest verifies 869 files, including the new fuel cue, previous witness briefings, the 21 custom radio tracks, and the six required original-game runtime directories. The cook retains the pre-existing warning about the editor map's obsolete `S:/Repos/.../CAPE WELLS.sc2` startup reference; it completed successfully and packages the actual city data.

Installed to `C:/SimCopterRemake/SimCopter Remake v1.0.1/Windows`. The installer copied the entire previous installation to `C:/SimCopterRemake/SimCopter Remake v1.0.1/Windows-before-gameplay-polish-2026-10-05`, then updated and SHA-256-verified nine changed files in place. It preserved 45 local city/save files and verified that all 21 custom radio tracks were unchanged. AppData profiles were not part of the installation operation.

Evidence: `Docs/scratchpad/gameplay-polish/installation.json`, `cooked-assets.csv`, `package.log`, and `install.log`. Launch the existing `Windows/SimCopterRemake.exe`; no new launcher is required.
