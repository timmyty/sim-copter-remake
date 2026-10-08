# Cow visual redesign (2026-09-30)

The user explicitly requested abandoning original-game cow fidelity after inspecting
its appearance. This supersedes the prior Coww part-slimming experiment.

- `Private/Ground/SimCopterCowGeometry.inl` builds a new black-and-white cow with a
  continuous ellipsoid torso, neck/head/muzzle, ears, eyes, horns, tapered legs,
  cloven hooves, udder, and hanging tail. Coat markings are face colors, not
  overlapping geometry or external textures.
- `BuildClipSections` selects this only for exact figure name Coww, bypassing its
  legacy per-part adjustments. Other figures retain the original path.
- Existing DgSt/DgRn clip selection and frame timing remain. The replacement has
  its own diagonal-pair gait and idle tail motion. GroundAgent does not apply the
  legacy pose-based ground lift to this model, which is built from a ground plane.
- Uses the existing vertex-color material and procedural frame sections; no new
  assets, plugins, packages, or cooking requirements.
- CowGeometry automation validates all 10 frames: finite vertices/unit normals,
  valid triangles and outward winding, grounding, bounds, and frame visibility.
  Its PLY exports in Docs/scratchpad/cow-mesh feed render_cow_mesh.py, so previews
  show the actual C++ mesh rather than a separately approximated model.

Validation and installation results are recorded in Docs/PlayableBuild.md.
