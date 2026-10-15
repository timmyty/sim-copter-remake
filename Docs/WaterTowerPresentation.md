# Water tower presentation — October 15, 2026

WT235 (GEO object `0x01c`, city tile 235) now uses painted ivory tanks, blue trim,
a restrained brass stripe, sheet seams, rivets, weathering and darker steel supports.
City-name lettering appears on two opposing sides. Original career paths resolve to
the same 30 names as the city picker; imported maps use their own CNAM name.

The presentation copy retains all 61 source vertices and 55 original polygons,
and now adds a shallow closed metal lid, roof vent, ladder and support bracing.
The lid is neutral galvanized steel, including its perimeter. The original tank
and leg proportions and placement remain intact. WT220 and WT244 are different
utility models and are not altered. This replaces WT235 wherever the original
city dispatch loads it, including imported maps.

## Performance and lifecycle

- Retains the original instanced-building component and one tower material slot,
  plus its existing separate foundation slot.
- Fewer than 1,500 rendered triangles per tower, including the existing reverse
  faces and foundation; the new details remain in the shared tower mesh.
- One material instance and one 1024×256 RGBA8 mipmapped lettering mask per city,
  created lazily only when that city contains towers. The mask is about 1.33 MiB.
- All towers share the same texture; no text actors, decal passes, per-tower render
  targets, ticking, scene captures or runtime readbacks are added.
- Label generation happens once during city construction. City reconstruction
  releases its previous material ownership through the existing cleanup path.
- Demolition removes tank, frame and lettering together. The merged fallback also
  uses the same material resolver.

`Tools/Unreal/BakeWaterTowerPresentation.py` creates `M_WaterTower` under the existing
always-cooked `Generated/CityAtlas` directory. Run it as an Unreal Python commandlet
with `-AllowCommandletRendering`. `BakeCivicPresentation.py` also calls this bake.
Both landmark materials retain the outward mesh normals on reversed legacy faces,
avoiding black lids under daylight. Original source assets remain unchanged.

## Validation

`SimCopter.WaterTowers.GeometryAndNames` checks immutable original geometry, UV seam
continuity, surface classification, all 30 career names and imported-name isolation.
`SimCopter.WaterTowers.CityIntegration` constructs sixteen towers through the real city
builder, checks sharing, material slots and triangle counts, exercises demolition and
reconstruction, and exports offscreen D3D12 captures and the actual lettering mask.
The original paint-only release results are recorded in
`Docs/validation/WindowsRelease117Results.json`. The closed-lid follow-up is included in v1.0.18 and documented in [Civic and waterfront review](CivicAndWaterfrontReview.md).

The v1.0.17 paint-only 1600×1200 offscreen comparison used the same sixteen-tower mesh, lighting,
camera and foundation, with GPU completion synchronization and ABBA material order.
Across 24 samples per material, the old material measured 12.014 ms median / 12.929 ms
p95; the new material measured 4.414 ms median / 12.584 ms p95. This is a controlled
capture workload, including synchronization overhead, not an interactive-flight FPS
measurement. The fixture verifies one tower finish slot plus its pre-existing
foundation slot, 248 combined triangles, shared lettering, demolition and rebuild.
