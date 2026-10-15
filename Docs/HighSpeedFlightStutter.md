# High-speed airplane stutter — October 14, 2026

The 8:18–8:20 PM CDT Heven 2026 capture was recorded after the earlier mouse-input and NPC animation-cache fixes were installed. The interrupted chat opened in the original game's directory, but its investigation and exports were already in this remake. It made no additional gameplay changes. Evidence is preserved in `Docs/scratchpad/fast-flight-2026-10-14/`.

## Evidence

`Witness-Performance_20261014_201846.utrace` records a 618.929 ms engine tick at trace time 51.599, immediately after the city introduction. That tick includes 265.467 ms of figure construction and 290.590 ms of deferred OriginalMesh updates. At 100.976 seconds, during the later flight segment, a 359.701 ms tick includes 236.238 ms of figure construction inside population spawning and 95.238 ms of deferred OriginalMesh work. Helicopter actor ticks total only 0.566 ms in that latter frame.

These are CPU timings from a Development capture with tracing and GPU crash diagnostics enabled. Nested timer totals overlap and must not be added indiscriminately. The 35–115 second summary includes introduction/menu time; it is not an uninterrupted flight FPS measurement. The log records normal shutdown and separate, pre-existing texture-streaming metadata ensures during initial city loading.

## Corrections

- Ambient vehicles and scripted pedestrians use deferred construction with their final kind, figure, clothing, root path and speed configured before BeginPlay. Previously BeginPlay built a default pedestrian and its prepared idle/walk clips before ConfigureAgent discarded them. Ordinary configuration, save restoration and worlds that have not started play retain immediate initialization by default.
- Population refill batches drain across successive frames, with at most one pedestrian and one vehicle created per refill pass. The leading-edge tile queue retains unfinished work while stationary, drops tiles outside the current local radius, and revisits multi-person service posts. Existing population caps, spawn eligibility, mission ownership and collision checks remain in force. The queue is transient and reconstructed after loading; the save format is unchanged. Mission spawns outside the ambient refill pass remain immediate. Automatically created baseball games also finish their initial roster through the refill scheduler; their independent tick cannot bypass that initial budget. Tests wait for all eight retail players or ten active full-game players and still check uniqueness, distance culling and return.
- Cow ellipsoids share vertices within each coat color. All 11,160 triangles per pose are retained, including their positions, normals, winding and markings. Each pose uses 8,513 vertices instead of 23,840; the ten prepared poses use 85,130 instead of 238,400. The existing untextured material does not require the old per-face UV/tangent seams. Immutable model-space cow poses are reused across respawns with a 16 MiB bounded cache; each actor receives its own scaled arrays. Animation timing, gait and silhouette are retained.
- `SimCopterPopulationSpawnPass` is a new CPU trace marker for follow-up captures.

## Verification

The initial deferred-spawn regression failed 16 assertions about premature mesh construction. Verification subsequently caught a distinction between unfinished construction and a world that has not begun play; the explicit deferred-initialization argument fixes both paths.

The offscreen four-cow benchmark, including deferred render work, fell from 287.129 ms before the geometry optimization to 58.212 ms afterward with the pose cache warm. This is a controlled construction workload, not a live-flight frame-rate claim. Timing varies with system load.

All ten before/after PLY exports have identical expanded triangle-position and color hashes. `cow-geometry-comparison.json` records those hashes. Automated checks additionally cover normals, winding, grounding, scale changes, retained triangles, vertex counts, cached animation transitions and saved animation state.

The moving-city regression crosses multiple Heven tiles, pauses to drain queued work and executes 120 refill passes with deferred render flushing. It checks actual created actors, continued replenishment and bounded pending tiles. The original implementation fails the burst assertions: 243 actors were created in 20 burst frames, with a 244.101 ms worst pass. The same isolated fixture with the fix creates 230 actors across 115 frames, with a 26.242 ms worst pass (89.25% lower). Total measured refill work is 1,662.315 ms before and 1,458.197 ms after. Actor identities and counts differ because refill is paced and stale exposed tiles are discarded as the focus moves; this is not an identical actor replay. The full validation and installation receipt is `Docs/validation/HighSpeedFlightStutterResults.json`.

Editor, Shipping and Development builds succeeded. All 559 distinct automation cases have passing final results: the broad sweep initially found three baseball roster cases, then the roster continuation correction and updated pacing assertions passed in the final 47-case rerun covering baseball, performance, population, save games and cow geometry. Both the earlier 20-case focused run and final 47-case rerun used D3D12 offscreen rendering. The broad sweep used NullRHI, where visual-only checks may be no-ops. The normal and diagnostic executables and symbols were installed with verified SHA-256 hashes and recoverable backups; the installation timestamp is in the receipt.

## Remaining verification

The exact interactive airplane route has not been replayed. Repeat that flight with the installed `Launch-With-Performance-Diagnostics.cmd`, including several fast crossings and a return to the same area. Inspect the new population marker, figure-build scopes and deferred mesh updates in the resulting trace. Dynamic light processing, uncached non-cow animations, immediate mission/scripted spawns outside the refill pass and GPU rendering can still contribute to frame time. This change addresses the measured population-creation bursts; it does not establish that every source of stutter has been eliminated.
