# City surfaces and pedestrian collision

Updated 2026-10-05.

## Appearance

Land and buildings retain the original art and layout, with smooth colour filtering, distance
mipmaps, and world-space surface detail. Land gains fine grain, broader colour variation and
roughness variation. Masonry and roofs gain subtle grain and normal relief, including surfaces
that were previously flat vertex colours. Window glass and mesh pools are excluded from masonry
relief. Terrain detail still fades at shorelines and building pads, and Low Power skips grain.

The five city atlas pages and their night variants have separate filtered copies. Each sample
clamps to its own atlas cell at both mip levels, and sampling stops at mip 5, where each cell is
one texel. Uncompressed colour prevents BC blocks from blending neighbouring cells at coarse
mips. The original 82 texture/mask assets remain byte-identical, preserving window IDs, original
water frames, sprites and their existing consumers.

Dedicated parents are `M_SimCopterCitySurface`, `M_SimCopterLandSurface` and
`M_SimCopterCitySolidSurface`. Old serialized city defaults migrate when a city rebuilds;
custom material overrides remain respected. Vehicle paint retains its existing parent.

`Tools/Unreal/UpgradeCitySurfaces.py` bakes and binds these assets using the existing original
textures. `BakeCityAtlas.py` invokes the upgrade after decoding art, and the material generator
also rebinds the generated instances. Filtered textures are refreshed by export/import in place.

## Collision

Runtime building meshes already had cooked complex-as-simple collision. The reproduced bypass
was in pedestrian movement: BHAV 308's `MoveThroughWalls` flag disabled both wall checks, and
kinematic frame movement could travel farther than the behavior VM's checked logical step.

Physical walls now block regardless of that legacy recovery flag. Walking, guidance, band
movement and external pushes sweep their actual frame displacement against city geometry.
A small contact margin prevents repeated steps from clipping into walls. Initially overlapping
people can move outward, but cannot move farther inward. Queries preserve the existing exclusion
of other pedestrian capsules and foliage and use the rendered building shape, keeping overhead
clearance and demolition behavior intact.

This is an intentional remake change to physical collision, not a claim of original behavior
parity. The remaining original cell-rule and height-recovery behavior is retained.

## Validation

- Editor and Win64 Shipping builds succeeded through `RebuildUnrealCpp.bat`.
- 33 targeted automation tests passed, zero failures or unrun tests. One city-fixture test emitted
  static-foliage attachment warnings because its newly spawned root is movable; building
  collision, demolition and rebuild assertions passed.
- `SimCopter.Collision.BuildingWalls` exercises real runtime meshes and ISM physics: walls from
  both sides, open space above them, recovery behavior, a long push, repeated tiny steps, initial
  penetration escape and removal of one instance without losing the other instance's collision.
  The original recovery behavior failed the new regression before the fix.
- `SimCopter.City.BuildingDemolition` now builds its own Islandtown fixture instead of borrowing
  an editor map with a stale absolute path. It tests actual building removal, rubble, surviving
  buildings and restoration by rebuilding.
- The final materials baked with no shader-compilation errors. The offscreen comparison attempt
  produced empty captures and is **not** visual validation. Live gameplay appearance and movement
  have not been checked interactively.

Evidence, original-file backups, source diff and test reports are in
`Docs/scratchpad/city-surfaces/`. `tests-verified.json` summarizes the final suite. The empty
capture files are labelled `invalid-empty-*` to prevent their being mistaken for previews.

## Installed package

The Win64 Shipping cook/package succeeded and its manifest contains all three new material
parents and the filtered texture copies. The installer verified 869 loose runtime files and
the six required original-data trees, then updated eight changed files in
`C:\SimCopterRemake\SimCopter Remake v1.0.1\Windows`.

The complete prior installation is backed up at
`C:\SimCopterRemake\SimCopter Remake v1.0.1\Windows-before-city-surfaces-2026-10-05`.
The update preserved 45 local city/save files and all 21 custom radio tracks. Installation
hashes and details are recorded in `Docs/scratchpad/city-surfaces/installation.json`.
