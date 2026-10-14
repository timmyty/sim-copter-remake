# Larger sky discs, varied parks and paved foundations — 2026-10-14

The user requested larger sun/moon discs, a detailed moon, park tiles without a pond every time,
and believable paths/paving in place of the yellow dashed grid under buildings. Scene brightness
must stay unchanged.

## Runtime changes

- `USimCopterMoonDiscComponent` sets the sun source angle to 3 degrees (previously 0.5357) and
  celestial MoonScale to 14 (CityRender previously 7.6913). The source-angle shadow factor is
  compensated to retain the original shadow softness. Light intensity and exposure are untouched;
  the component's brightness default now matches CityRender's existing disc brightness of 10.
- A hard-referenced project material, `Generated/CityAtlas/MI_SimCopterMoon`, retains the plugin's
  matching 1024x1024 albedo/normal maps and spherical phase shader. Contrast 0.06, relief 0.6,
  earthshine 0.008 and transition 0.85 improve visible surface detail. The MID preserves MoonAge
  through updates and reconstruction. Absolute scaling prevents per-tick growth; paused ticking
  supports Photo Mode. Engine plugin assets and CityRender itself are unchanged.
- `SimCopterScenery` creates six presentation variants of original LP13 / object 0x143 / tile
  0x0D. Only variant zero retains its seven water polygons. Other variants rearrange two or three
  trees over the existing grass, with straight, T and cross gravel paths on the last three.
  The tile-coordinate hash is stable and does not consume simulation RNG. Original mesh library
  data remains immutable; instance keys distinguish all six layouts. Other tree-density tiles
  remain unchanged (they did not contain these ponds).
- Separate foundation objects 1–4 and 0x164–0x16B receive `M_SceneryPaving`. The yellow grid
  was deliberate legacy artwork: commercial foundations repeated page 2 cell 10, a helipad
  crosshair, across their UVs. New world-space running-bond paving removes that repetition.
  Existing roads and rooftop/hospital helipads retain their own materials.
- `Tools/Unreal/BakeSceneryPresentation.py` reproducibly creates the paving, gravel path and moon
  materials. The existing Generated/CityAtlas always-cook rule includes all three assets.

## Validation and local installation

Editor, Shipping and Development builds succeeded through `RebuildUnrealCpp.bat`. All seven
distinct final focused automation cases pass without test warnings: the three Scenery cases,
TreeGrounding, SurfaceShadingDefaults, DayNightFog and DayNightLength. The scenery case builds a
real SC2 fixture through the city actor and verifies the actual instanced material assignments.
The sky case verifies unchanged light intensities, stable scale, retained lunar phase, MID
reconstruction, paused ticking and visible neutral lunar detail in full/quarter/crescent captures.

Offscreen D3D12 captures of the park layouts/paving and all three lunar phases were inspected.
Capture-fixture corrections were necessary: persistent rendering/exposure state, an initial draw
before finishing on-demand shaders, a camera-facing moon plane, and overhead atmospheric moonlight.
The default synthetic celestial actor otherwise gives a below-horizon red tint. Those fixture
changes do not change gameplay exposure or light intensity. The preview moon is deliberately
framed as a close-up, not a demonstration of its gameplay screen coverage.

Shipping cook/package/archive succeeded; the UFS manifest includes all three new material assets.
The installed normal and diagnostics executables/symbols and five pak/IoStore files were updated
after checking that the game was closed. All nine files have verified backups and SHA-256 hashes
in `Docs/scratchpad/celestial-presentation/installation.json`. Saves, music and loose user maps
were not replaced. No interactive gameplay check or publication update was performed.

Both installed executables survived separate hidden 30-second NullRHI startups with isolated
profiles. The Development log repeats the pre-existing CelestialVault class-default missing
`MI_StarsStar_EnergyConservative` diagnostic, also present in the earlier loading-presentation
startup; CityRender uses its own stars material override. No new startup failure was observed.

Evidence: `Docs/scratchpad/celestial-presentation/`; receipt:
`Docs/validation/CelestialAndSceneryResults.json`.
