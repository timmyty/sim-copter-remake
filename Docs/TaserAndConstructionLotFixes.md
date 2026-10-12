# Remote taser visibility and construction-lot surfaces

October 11, 2026.

The helicopter's remote sight now displays its white electrical discharge and
vehicle/pedestrian shock arcs. CA137 construction lots no longer alternate between
their gray floor and underlying textured base.

## Causes and changes

Entering the remote sight adds the helicopter to the player controller's
`HiddenActors`, preventing its fuselage from blocking backward aim. The previous
procedural trail and shock mesh belonged to that same helicopter, so Unreal hid
both effects too. Their absolute world transform did not exempt them from the
controller's hidden-component list.

The effects now belong to a separate transient, unattached actor. They retain their
existing lifetimes and moving-target behavior, and helicopter component teardown
destroys the effects actor. The firing trace, eight-second vehicle disable,
pedestrian arrest rules and sound cues retain their existing behavior.

The first actual muzzle-view render also exposed a near-camera problem: both
solid electrical strands started inside the sight's eye and projected as a large
white square. The strands now leave the two electrode positions, with a narrow
taper near the camera, keeping the target visible during the discharge.

The pictured object is the original `CA137`, SIM3D1 object `0x09b`, placed by XBLD
`0x89`. Original face 32 is a gray floor at Maxis Y=0, except for one corner at
0.0184 units. The original zone-base selection (`FUN_00482890`, already documented
in the city importer) adds a textured base at Y=0. Their coincident triangles cause
depth-buffer flicker. Only this model's gray ground face is lifted 2 cm in the
shared city import path. Its base, posts, raised block surfaces, texture mapping
and footprint remain intact. Imported collision follows the adjusted floor.

## Evidence

- The original remote-sight render measured **zero** visible discharge pixels and
  **zero** vehicle-shock pixels. The corrected render measures **6,195** and **574**
  respectively at 1280x720, with the real remote mode and hidden-airframe state.
- With floor separation disabled, all four two-sided gray-floor triangle checks
  fail, and the rendered construction lots show the same triangular overlap as
  the supplied screenshots. The corrected geometry passes; two nearby viewpoints
  were saved for visual inspection.
- The earlier side-on taser preview passed because it bypassed the actual remote
  sight's hidden-actor state. The revised preview retains that coverage and adds
  an actual firing shot from the muzzle camera.
- The first lot fixture used Islandtown, which has no CA137 placements; the test
  now uses city0, which has eight. The explicit component teardown fixture now
  pairs `BeginPlay` and `EndPlay`, as required by Unreal.

Build, regression and installation results are recorded in
[TaserAndConstructionLotResults.json](validation/TaserAndConstructionLotResults.json).
Reproduction logs, source snapshots, the focused diff and rendered images are in
`Docs/scratchpad/taser-and-lot/`.

Validation uses headless gameplay tests and D3D12 offscreen renders. No interactive
flight, gameplay or listening test is claimed. The publication checkout is not
changed by this local fix.
