# Marina docks and shore connections — October 18, 2026

The reported screenshots show MA248 (object `0x0f3`, XBLD `0xf8`), not the separate AP223/AP223F pier models. MA248 combined a colonial marina house, flat red T-shaped walkways, and secondary object `0x166`: a full three-by-three residential paving foundation. The earlier AP223 timber change did not address that marina.

The marina now retains its original house walls, roof, porch, and atlas coordinates, with actual timber boards, support beams and pilings replacing the red apron and walkways. The deck is 40 cm above its terrain/water datum. The residential paving foundation is omitted. Both marina and separate pier models use `M_DockTimber`, a shared rough timber material with filtered grain and board tone variation.

Each valid marina footprint samples the surrounding terrain before the renderer and water gameplay grids are populated. Shore-adjacent cells remain dry and match their neighboring land; road-adjacent cells get a paved approach; water-facing cells become open water. Corner shore access wins ties, while the middle cell under the house remains water if any equally close side is water. An all-land marina stays on land. Neighboring marinas are traversed to find the actual shoreline, using an immutable terrain snapshot. No terrain outside the marina footprint is changed. Road approach masks participate in model caching, so unlike sites do not accidentally reuse each other's pavement geometry.

This is a requested presentation change. Original model identity and placement/footprint dispatch are grounded in `FUN_0047c0c0` and the shipped MA248 geometry. Original collision generation, water animation, road geometry, and building atlas rendering remain the shared project paths.

Implementation: `SimCopterMarina.h/.cpp`, city integration, `BakeDockTimber.py`, and the generated material. Regression coverage exercises sixteen shoreline combinations, road/water boundaries, adjoining marinas, invalid footprints, retained architecture, model bounds, actual instanced material binding, terrain gameplay sampling, and an open-slip collision trace. Offscreen captures show a shore-connected marina, an open-water marina, and the separate pier.

All 44 selected regression cases passed with offscreen D3D12 rendering. The inspected captures confirm the timber appearance and separate land/road/water treatments. No interactive gameplay was performed.

Editor, Shipping, and Development builds and packaging succeeded. The final two-case marina rerun also passed after widening the pier camera framing. One concurrent preview launch stalled during editor startup before running tests; that task-owned process was stopped and the rerun completed after the builds. The timber material was cooked for both SM5 and SM6.

Installed seven changed runtime files in `C:/SimCopterRemake/SimCopter Remake v1.0.1/Windows`, with verified backups and matching hashes. All 183 checked maps/save/config/optional-media files were unchanged. Both installed executable variants survived separate 30-second hidden startups with isolated profiles. The James installation and publication checkout were not modified.

![Marina meeting land and road](scratchpad/marina-docks-2026-10-18/marina-shore.png)

![Marina surrounded by water](scratchpad/marina-docks-2026-10-18/marina-water.png)

![Separate pier](scratchpad/marina-docks-2026-10-18/pier.png)

Exact build and installation status is recorded in [the validation receipt](validation/MarinaDocksOctober18Results.json). Scratch artifacts, baseline copies, logs and preview images are under `Docs/scratchpad/marina-docks-2026-10-18/`.
