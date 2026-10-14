# SimCopter map selection, Base Location and the transport record

*Decoded 2026-09-23 from the Ghidra exports (`.ghidra-exports/*.json`) and ported the same day.*

What the cockpit map points at, who decides, and what a transport record holds. The original
answers all three in the mission layer, not in the map.

## The Base Location record (mask 0x100000)

- City entry `FUN_0047a240` resets the table (`FUN_004a6c80`, which also sets `DAT_0057f9d8 = 0`).
  Its last call is `FUN_004a7a10(DAT_005d91d0, DAT_005d91d4, 0x100000)`.
- `DAT_005d91d0/d4` = airport origin (`_DAT_005d91b0/b4`) **+ 1**, written by `FUN_004829f0`.
- The 0x100000 branch of `FUN_004a7a10` does `sprintf(rec, "%s", DAT_005816b8)`. It then sets
  `+0x20 = 0`, `+0x24 = -1` (no event id), category 0, and leaves `+0x30`/`+0x38` at -1.
- Mask 0x100000's type/voice id is **0x24a = string 586 "Base Location"**. 0x23b..0x24b map to
  strings 571..587; 0x24b is "Non-Mission Event". The remake used to call this bit **"UFO". That
  was wrong**: the flying UFO is ambient plane slot 1 and is paid through `EVT_UfoResolved`.
- The shared tail of `FUN_004a7a10` adopts the new record as the selection when none exists, so it
  is slot 0 and selected. The same tail also:
  - counts it in `DAT_0057f9c8` (active jobs). The scheduler cap Max Easy + tier therefore always
    has one slot taken. **The remake's scheduler leaves it out (deliberate divergence, 2026-09-23,
    `UpdateSchedulerCadence`)**: at tier 1 (career cities 0-8 and 10, cap 3) the faithful count
    meant one job at a time and a full 380 s Easy Interval wait for a second, because the IntervalAdj
    speed-up is zero with only one slot free. That played as long idle stretches. Logged as
    divergence 3 in [[simcopter-pacing-divergences]].
  - posts the kind-5 message 0x24a.
  - sets `DAT_00505fb4 = DAT_00505fac`.
- `FUN_004a73e0` wraps everything in `if ((rec[0x50] & 0x100000) == 0)`. The record only ages; it
  never completes, expires, scores or gets adopted by the lifecycle.
- `FUN_004a4000(0x100000)` = -1/-1: no map icon. It is visible only when selected, as line A to the
  base.
- `FUN_004a92f0` and `FUN_004ab480` have no 0x100000 case.
- `.data` initial values (read from the exe; `.data` raw offset 0xfdc00 for VA 0x500000) are all
  zero for `DAT_00505fac`, `fa4`, `fb4`, `f0c`, `f10` and `f08`. The tweak loader and
  `FUN_004a6c80` set them.

## DAT_0057f9d8 - the map's selected record (a record pointer)

Every write, exhaustively (xrefs to 0x0057f9d8):

| Where | Rule |
| --- | --- |
| `FUN_004a6c80` (city reset) | = 0 |
| `FUN_004a7a10` / `FUN_004a9a10` tail | if null and new record's category != 2: = new |
| `FUN_004a73e0` live arm (cat not 2/4/8, not 0x100000) | if null: = this record |
| `FUN_004a73e0` completion (cat 8, all goals met, cat-2 jam `cleared == count`) | if it was the selected record: = 0, then the first slot with bit0 set and +0x54 != 2 |
| `FUN_004a73e0` cat-4 arm and jam 0x5a0000 expiry | **no write**: a failed/expired selected record STAYS selected (stale) |
| `FUN_004a9860` / `FUN_004a9900` (commands 0x1e / 0x1d in `FUN_004796c0`; map buttons via thunks `4a3ed0`/`4a3ec0`) | next/prev live non-cat-2 slot, wrapping; no change if null or nothing else |
| `FUN_004ab3e0` (load) | = first live non-cat-2 slot |

- The event sink `FUN_004a89c0` / `FUN_004aa150` never writes it, so a **pickup, a delivery or a
  timeout does not change the selection**.
- The getters `FUN_004a8960`/`8940`/`8990` test only the pointer and the -1 coords, not the active
  bit. The map therefore keeps drawing a stale record until the player cycles or a new record
  reuses the slot.
- Failure paths that post `EVT_SetCategory` 4: `FUN_004b2cd0` (plane whose fire became its own
  mission) and `FUN_004b8b60` (failed criminal-car arrest).
- There is no generic mission-timeout expiry in `FUN_004a73e0`. See
  [passenger failure](simcopter-passenger-mission-failure.md).

## What the map draws

- **`FUN_004a3820`, the selected record.**
  - Line A (shade `0x3f - 16*len/0x184`) goes to `+0x30`, else `+0x28`.
  - Line B (`0x6a - 8*len/0x184`) goes to `+0x38` when it is set.
  - **No icons** (icon -1), and no "begun" swap.
  - With nothing selected it draws only the heading needle.
- **`FUN_004a4200`, the other records.** It covers every live, non-cat-2, non-selected record.
  - Icons from `FUN_004a4000(exact mask)`: the first at (`+0x30`, else `+0x28`), the second at
    `+0x38`.
  - They are drawn along an invisible ray, so an off-screen record is pinned to the edge.
  - They are gated by `DAT_00505f0c` (the marker toggle) inside `FUN_004a3f20`.
- **0x40 = (-1, 3).** A transport shows no icon at its drop-off and icon 3 at its pickup only. Once
  picked up, an unselected transport shows **nothing**.
- The label (`FUN_004547a0`) prints the selected record's name and is blank when nothing is
  selected. At city start it reads "Base Location".

## The transport record (0x40, `FUN_004a7a10`)

- `+0x28` = the placer's tile, a mission building = the **destination**.
- `+0x30` = copy of `+0x28` = the drop-off. `FUN_004a88e0` hands it to BHAV 292.
- `+0x38` = the **pickup**. `FUN_004a7a10` draws up to 10 times with `FUN_004abb30(+0x28)`; the
  first draw on XBLD 0x70..0xdb wins, and 10 misses fail the creation. The party
  (`rand % (tier+1) + 1`) spawns here, and `FUN_004ab480` announces here.
- `FUN_004a73e0` clears `+0x38` when `pickedUp(+0xa4) + dead(+0xb4) + lost(+0xbc) == passengers(+0x88)`.
  It completes the record on `delivered(+0x9c) + dead + lost == passengers`.
- **Medevac (0x20) leaves `+0x30` at -1** and puts the patient at `+0x28`, which is cleared on
  pickup. Since 2026-09-23 the remake does the same: any hospital takes the patient (see
  [paramedic handoffs](simcopter-paramedic-handoffs.md)). Since 2026-10-13 the map panel adds a
  nearest-hospital guide to its display copy of the selected record; the real record still has
  no assigned hospital (see below).

## The four player-reported claims (verdicts)

1. **"Base Location mode by default: a line to the base, icons for live jobs."** CONFIRMED. The mode
   is simply the Base Location record being selected.
2. **"Transport shows only the pickup until pickup, then the drop-off."** CONTRADICTED.
   - Unselected: icon at the pickup only, then nothing.
   - Selected: lines to both from the start, then the drop-off line only.
3. **"The map switches to the transport on pickup and back to base when it finishes."**
   - Switching on pickup: CONTRADICTED.
   - Back to base on finish: CONFIRMED, but only when the finished record was the selected one. The
     completion re-pick lands on slot 0, which is Base Location.
4. **"The map switches away on timeout/failure."** CONTRADICTED. Category 4 and jam expiry leave
   the selection stale.

## Remake port (2026-09-23)

- `FSimCopterMissionSystem` owns `DAT_0057f9d8` as `FocusRecordIndex`. All writes go through
  `SetMapFocusRecordIndex(slot, EMapFocusReason)`, the **hook for remake-only selection rules**.
- `EnsureBaseLocationRecord` is called from actor `BeginSession` and every tick. Every tick covers a
  late airport and old saves.
- **Deliberately not ported:** the base record's kind-5 message (no "Base Location started" in the
  ticker or career log) and its countdown reset.
- The transport layout matches the original. Saves in the old layout (`+0x28` = pickup,
  `+0x30` = destination) are migrated on load.
- The mission catalog's "UFO" row is removed, because it only created a second base record.
- Tests: `SimCopter.Missions.BaseLocationRecord`, `.MapFocusRules`,
  `.SaveFocusAndTransportLayout`, `.MarkerCoordinates`, and `SimCopter.Map.Overlays`.

## Remake-only rules layered on top (2026-09-23)

Chosen by the project owner over strict fidelity; both go through `SetMapFocusRecordIndex` with
their own `EMapFocusReason`, so they are easy to find or drop:

- **`PassengersAboard`** — `EVT_VictimPickedUp` on a transport (0x40) selects that record, so the
  map follows the job the player is now flying to its drop-off. The original never writes
  DAT_0057f9d8 on the pickup path.
- **`Expired`** — `RefocusAfterExpiry` runs on the category-4 and jam-expiry arms and re-picks the
  first live record (Base Location) when the dead record was the selection. The original leaves
  that stale selection on the map until the player cycles.

`SimCopter.Missions.MapFocusRules` asserts the remake behaviour for both.

## Nearest-hospital guide for selected MedEvac missions (2026-10-13)

User-requested remake addition: `SSimCopterMapPanel::BuildFrame` adapts the selected, active
medical mission's display copy. Its white line points to the closest hospital's building center
and its red line points to the waiting patients. After pickup clears the patient coordinates,
only the hospital line remains. Selection and mission completion keep their existing rules.

`GetHospitalSites` supplies actual hospital footprints. Each map refresh chooses by squared
horizontal world distance from the map's current player pawn (helicopter or on-foot pilot),
ignoring altitude, then converts the winning center through the traffic system's city transform.
No hospital means the original patient guide remains; after pickup there is no invented target.
The same raster provides fading, zoom, offscreen clipping, and visibility with blips disabled.
Existing police-custody routing retains its priority.

Only the selected frame copy gets Secondary/Tertiary coordinates. Unselected patient icons,
mission records, save layout, and acceptance at any hospital are unchanged. This is deliberately
different from `FUN_004a3820`'s unmodified retail record inputs, verified against
`.ghidra-exports/004a3820.json` and the existing `FUN_004a7a10` record port.

Regression: `SimCopter.Map.MedevacNearestHospitalGuide` builds actual panel frames and checks
raster pixels at all four zooms, horizontal nearest selection and movement, pickup and completion,
selection cycling, missing/removed hospitals, and preservation of the mission record layout.
Validation receipt: [MedevacMapGuideResults.json](../validation/MedevacMapGuideResults.json).
