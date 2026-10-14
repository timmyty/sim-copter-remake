# SimCopter hangar shell

*"Where the hangar shell's text and layout actually live (exe STRINGTABLE, not .rdata), and the traps in the catalog/upgrades/inventory art"*

*Recorded 2026-07-26; ported into the repo 2026-07-29.*

**2026-10-11 reference reconstruction:** The world shell now follows the supplied catalog illustration. `Private/City/SimCopterHangarStructure.cpp` builds six structural/detail sections and separate non-shadowing frosted glazing: tall side-window grids, timber roof and skylight, steel trusses/I-columns, rear office windows/conduits/gallery/stairs, lamps, barrels and both caution signs. `SimCopterHangarGeometry.h` shares the native geometry helpers. The terminal footprint stays 1.9 by 1.9 tiles; eaves/ridge rise to .80/1.14 tiles, and the doorway is 1.80 by .77 tiles. The old airport atlas skin/resolver was removed from this actor.

`SimCopterHangarInterior.cpp` retains eight non-colliding scenery sections: brown four-headlight sedan, red tool cabinet, desk/trophies/safe, blue chair, bookcase, crates/cart, original PLANE1 and parked Jet Ranger. Furniture now uses the pilot's quarter-real-world scale; the original props were over twice his height. Aircraft use native palette-corrected GEO solids and stationary rotors, with noses toward the left of the doorway view, and sit behind the foreground car/desk groups. All props scale with the tile; the entry and apron stay clear. Physical window/workshop fills and a modest material ambient floor track daylight without the former inverse-exposure bleaching. This reconstructs the layout in the game's polygonal style, not the still image's photographic texture/volumetric effects. See [details](../HangarReferenceMatch.md).

**Geometry lighting trap:** UE's visible clockwise triangle winding is opposite the cross-product convention used by the helper. A two-sided sheet needs separate vertices/normals for each face with correctly paired winding. Reversing only normals made the floor point down and rear wall point out, leaving the interior almost black. The render test now checks the actual world-normal pixels (rear wall +X, floor +Z), not just whether any bright trim remains visible. Repeated fourth corners for triangle caps/gables must omit the degenerate second triangle before collision cooking.

`SimCopter.City.HangarInterior` checks footprint/height bounds, rebuilds, pilot-scale furniture, entry behavior, structural sections and rendered visibility through the doorway from three exterior positions, plus occlusion from behind. Its lit capture checks each major prop under physical camera exposure and saves 2560x1440 doorway/exterior views. Install the fixture sun before spawning the hangar: the exposure subsystem caches missing-light scans for two seconds. Current captures are in `Docs/scratchpad/hangar-reference-match/`; earlier props-only captures remain in `Docs/scratchpad/hangar-interior/`.

Ported 2026-07-26 as `ASimCopterHangar` + `SSimCopterHangarMenu` (Private/UI) + `USimCopterHangarArt`
+ `SimCopterHangarShop` + `USimCopterCareerSubsystem`.

**The shell's text is in the exe's Win32 STRINGTABLE, not in .rdata.** Grepping
`.ghidra-exports/_strings.json` for "Bambi" or "Catalog" finds nothing — those live in RT_STRING
resources. Dump them by parsing the PE resource directory (script pattern in the session
scratchpad; ~60 lines of stdlib Python). English is resource language 1033 with ids 100..599;
the other five languages are the *same* ids plus a per-language block offset (French +2000,
German +3300, Italian +4000, Spanish +5000, Dutch +6000).

Id map worth keeping: 100-102/125-128 hangar buttons, 400-408 helicopter names *by runtime type*,
410-414 equipment names in inventory-column order, 420-422 + 439-441 letterheads, 430-444 catalog
labels/buttons, 460-467 History, 470-477 Specialties, 480-487 Description (all three **by catalog
row**), 490-494 upgrade blurbs, 500-521 weekday/month names, 530-532 log buttons, 570-587 mission
type names.

**The hangar goes on the airport's terminal plot** - the middle 2x2 of the block, i.e.
`SimCopterAirport::GetTerminalTile`, which the airport port already resolves
([[simcopter-airport-spawn]]). That is where the original puts its own airport building (XBLD 0xf6
-> GEO object 0x096 on base 0x165); standing the hangar beside the block instead just gives the
airport two buildings. Demolish the terminal first with `bLeaveRubble=false` - object 0x165 is a
flat 2x2 slab exactly on the apron, so leaving it z-fights the hangar floor - and lift the floor
2 cm, because with the slab gone it would then z-fight the terrain instead. Height needs no trace:
`FUN_004829f0` flattens the whole 4x4 to one sample, so the pad table's Z *is* the plot's Z (and a
trace there would find the terminal's own roof).

**Historical airport skin (superseded on the world hangar):** The original terminal textures are page **40** of `BMP/SIM3D.BMP`: object 0x096's fourteen
faces use cells 20..23 for its walls (cell 23 on five of them), and pad object 0x08b uses cell 61
for its slab. Its roof faces are type 15 - flat-shaded, no texture at all - so a roof cell has to
be borrowed (52 is the airport's gravel). Decode the page with the GEO pack's own CMAP palette
(sim3d1.max + SIM3D.BMP), `ExtractAtlasTile` cuts the 32x32 cell, and `M_SimCopterLitTexture`'s
`Texture` parameter takes it; set `AddressX/Y = TA_Wrap` and tile UV0 by length/170cm to match the
original's own UV density (its UVs run to 3.0 across a 478 cm side).

Art traps (all measured off the shipped BMPs, cross-checked against `FUN_0042a7d0`/`FUN_0042b840`):

- **There is no hangar building in the original.** `dhangar.bmp` is a rendered still and the shell
  is reached from a menu, so the hangar *mesh* is a remake invention. The plot and ground height
  come from the airport; the new world architecture and furnishings follow the still image.
- All nine `CAT_*T.BMP` tab strips draw the **same eight tabs in catalog-row order at the same x
  positions** — only the page corner behind them moves. One hit-rect table serves all nine. Tabs
  span x 12..467 inside the strip, and the strip goes at page (80, 420), which lands them exactly
  over the row printed on `catalog.bmp` at x 92..547.
- `CATALOGE.BMP` already has its three letterheads **and** all five equipment photos printed on it
  — draw only the blurbs. `INVNTORY.BMP` does **not**: its header band is blank paper and the
  diagonal column labels have only the rules, so both are runtime text (rotate the labels -45°).
- The upgrades page reads **column-major** (left column top-to-bottom, then right). That is what
  makes `FUN_0042d840`'s `{0, 1, 3, 4, 2}` row->equipment-index table line up; row order left-to-
  right does not.
- Helicopter prices are heli.twk's `New Cost` control (Ctrl11 in every shipped section), not a
  .data constant — the per-type block's `+0x44` is only a placeholder until `FUN_00489e20` runs.
- Palette index **254** (cyan) is the colour key across the sprite bitmaps (button strips, the
  tick), same as PEOPLE1. Full-page backgrounds load opaque.

Two engine traps hit on the way:

- `UPrimitiveComponent::OnComponentBeginOverlap` did not fire for a pawn that **teleported** into
  the trigger (console `BugItGo`, and presumably a respawn). The actor now also re-checks the box
  in a 0.15s tick — keep both paths.
- `FVector2D` is double-precision in UE5, so `TestEqual(TEXT(".."), Vec.X, SomeFloatExpr)` is
  ambiguous. Cast the expected side to `double`.

Verification recipe is [[simcopter-ingame-verification]]; `BugItGo <x> <y> <z> <pitch> <yaw> <roll>`
through the `~` console is the way to move the camera, since synthesized keys never reach gameplay
input.

## Equipment preview page — October 11, 2026

The user-requested equipment page replaces the printed equipment photos with six
selectable icon rows. It names the active helicopter separately from the preview.
Left/right keys, controller shoulders, and visible arrows browse revealed catalog
helicopters; up/down selects equipment. Browsing does not change the active pawn.
The existing shop's fleet-wide ownership supplies green icon checks and purchase
availability. Highlighting previews an unowned fitting without granting it.

`SimCopterEquipmentPreview` reuses the original GEO body, painted palette, rotor
hierarchy and per-aircraft tail offset, then adds a distinct fitting for each row.
The harness is shown suspended from the side winch; the bucket and cage also show
their deployed shape. All owned fittings remain present. A cached Slate projection
of that geometry supplies the right-hand view without a preview world, new assets,
or scene-capture ticking. Missing data produces a text fallback.

Explicit light button brushes keep dark labels readable in both core and editor
styles. Custom mesh vertex colors are encoded to sRGB. Offscreen page captures
use `FWidgetRenderer(false)` to avoid applying gamma twice, and must set the art
object's original-game root before rendering. `SimCopter.Hangar.EquipmentPreview`
checks all nine models/six fittings, navigation, preview-only state, ownership
updates, and actual page rendering. Evidence: `Docs/scratchpad/equipment-preview/`.

Editor and Shipping wrapper builds succeeded. All 24 selected headless checks
passed (23 clean; one existing hangar fixture with two world-context warnings).
The D3D12 equipment test passed after the final contrast correction, and all six
page captures were inspected. The Shipping executable was installed into the
local v1.0.1 game with a verified backup and matching SHA-256. No interactive
gameplay/listening check, cooked-asset changes, publication sync or push.
See [results](../validation/EquipmentPreviewAndAirportRecoveryResults.json).

## Equipment selection, fleet ownership and bumpers — October 11, 2026

Equipment rows no longer change selection on hover. Clicking a row selects it;
controller A moves to the available Buy/Sell action (Done when neither is available),
and a separate press performs the action. B returns to the item. Rebuilding after
a transaction preserves focus in the action row, and held A repeats cannot transact.
`SelectEquipmentAction` reads the shop row directly because Slate's enabled attributes
may still describe the previous item until the next prepass.

The earlier inventory presentation assumed global ownership but transactions only
changed the active pawn. Original FUN_0042d840 writes one career +0x48 equipment mask.
Purchases and sales now affect every owned player helicopter, charging once. New
deliveries copy exact ownership, including absent starter bits. The discovered Apache
inherits the owned fleet's equipment when claimed. Successful save restoration merges
old per-aircraft purchases across the restored fleet, excludes support/planes/unclaimed
aircraft and debug grants, and does not refill ammunition in an existing launcher.
Selling a cage is blocked if any owned aircraft still has it deployed.

The old LB+RB tool-wheel chord latched both steering inputs off until both bumpers
were released. X now owns Tools/Dispatch, so that legacy chord/latch is removed.
Opposed bumpers cancel while both are held, and the remaining one resumes immediately.
Normal single-bumper cruise steering was already symmetric: the regression drives
actual bindings through all nine flight models, forward held/released and Turbo
on/off. Four overlap assertions reproduced the latch defect before the fix; this
does not establish that every reported physical-controller failure had this cause.

Validation and installed-file hashes are in
[EquipmentControlsResults.json](../validation/EquipmentControlsResults.json).
