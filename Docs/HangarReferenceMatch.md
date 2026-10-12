# World hangar matched to the catalog illustration

The world hangar now follows the supplied interior illustration when viewed through its open doorway. The former low, dark shell and oversized furniture made the two views look like different buildings.

The new structure has a tall pitched timber roof with a central skylight, two bands of gridded windows on each side, steel roof trusses and I-section columns, pendant lamps, and a cream rear office wall with windows, conduits, gallery braces and stairs. The front-right column carries the two illustrated caution signs. Window grids also appear on the exterior faces.

The brown four-headlight sedan and red tool chest occupy the left foreground; the desk, three trophies, blue chair, bookcase and safe occupy the right. The plane and parked Jet Ranger sit farther back with their noses turned toward the left of the entrance view. Furniture now uses the same quarter-real-world scale as the pilot. This corrects the prior mismatch where the cabinet and desk were taller than the pilot.

The warehouse remains within the airport's central terminal plot, preserving the helipad ring. At the normal 400 cm city tile it measures 760 by 760 cm, with 320 cm eaves, a 456 cm ridge and a 720 by 308 cm opening. Scenery remains non-colliding; the walls, floor, roof and frosted glazing retain collision, and the existing catalog entry behavior remains in place.

Lighting uses ordinary physical lights plus a daylight-dependent ambient floor. The visible sides of generated surfaces now have the correct normals: the earlier winding/normal pairing made the floor and interior walls reject light from inside the room. The old inverse-exposure lamp and airport cobble/gravel wall skin have been removed from this hangar.

This is a native 3D reconstruction of the reference's architecture, layout and furnishings. The original catalog illustration is unchanged. The game retains its polygonal art style; the still image's photographic textures and volumetric light shafts are not reproduced pixel for pixel.

## Validation

Editor and Shipping builds passed through `RebuildUnrealCpp.bat`. All 11 selected headless cases passed, with only the existing contextless-world warnings in `SimCopter.UI.HangarPurchaseParking`. The offscreen render case passed, including both visible-normal assertions. The final doorway and exterior captures were visually reviewed. The installed executable was backed up and replaced with SHA-256 verification; no cooked asset replacement was needed.

Build, test and installation evidence is recorded in [the validation receipt](validation/HangarReferenceMatchResults.json). Local rendered captures and the before/after source diff are under `Docs/scratchpad/hangar-reference-match/`.

The focused checks cover prop bounds at multiple tile sizes, rebuild behavior, pilot-scale furniture, doorway access, shell detail sections, exterior prop visibility and rear-wall occlusion, visible-surface normal direction, and prop visibility under physical lighting. Lit doorway and exterior previews are rendered at 2560 by 1440. No interactive city playtest was performed.
