# October 18 playtest fixes

Local gameplay update for the 24 reported issues. The source includes the earlier Heven traffic fixes. No publication checkout or GitHub release was changed.

| Report | Result |
| --- | --- |
| Boat recovery lacked harbor acknowledgement | Opening the cage over a valid harbor delivery completes the mission and announces harbor acceptance. A release near the harbor that is too high, fast, or not yet sealed now retains the boat and explains what to correct, rather than silently dropping it without credit. |
| `superpowermultiply` descent too fast | Turbo retains the normal downward speed limit and acceleration. Upward boost remains available. |
| Respawn the active helicopter at the hangar | New case-insensitive cheat: **`Respawn helicopter`**. Recalls the active aircraft to a free airport pad, parked with engine off, full fuel and health, retaining owned equipment. An on-foot pilot boards it. |
| Moon caused a flickering horizon arc | The decorative moon mesh no longer casts shadows or participates in distance-field/ray-traced lighting. This addresses the likely shadow mechanism; the exact moving daytime arc has not been replayed interactively. |
| Emergency building lights too fast | Police/fire station beacon phases hold twice as long, about 0.6 seconds per color. Vehicle beacons retain their cadence. |
| No officer on police headquarters roof | Nearby hospital/police roof posts are serviced independently of random ambient population spawning. Existing pickup/drop cooldowns remain. |
| Police traveled too fast | Police vehicles use the original randomized traffic speed distribution from `FUN_0049dbb0`, including a cap for responders restored from older saves. |
| Repeating “Waiting for cops!” cash | Repeated pursuit/wait events award no money, score, or notification. The final arrest retains its reward. |
| Too many automatic EMS vehicles | Automatic body-recovery dispatch is capped at three circulating ambulances, including ambient units. Player-requested dispatch can still use the full service pool. Ambient refill does not replace ambulances already handling a call. |
| Band members spin repeatedly | Formation steering owns their yaw; behavior-script octants no longer fight it. Formation turns are limited to 90 degrees per second. |
| Clouds in introductory tours | All tours retain the preview-style cloud layer, including sandy maps. Existing late material replacement and low-power graphics handling is retained and tested. |
| Delivered living patients should celebrate | After a hospital accepts them and the carrier releases them, patients acknowledge/cheer for two seconds, wave for four, then despawn. The remaining farewell time is saved. |
| Ambulance medic stuck near a body/building | Recovery medics use the existing obstacle-aware medical paths, a reachable interaction radius, and a small waypoint stopping tolerance. Promoted ambient ambulances choose a reachable hospital. The body and ambulance retire after returning there. |
| Dock ground and water | Pier tiles retain timber plank/piling geometry and now have water underneath and around the walkway, even when the imported tile was classified as flat land. |
| Endless fall after exiting below the world | At 50 meters below the city origin, the active aircraft is recalled and the pilot is returned inside it, provided an airport pad is available. |
| Fast helicopter crosses building collision | The complete movement sweep stops at solid geometry and writes the safe position back into the flight model. The rejected endpoint is no longer applied as an unswept teleport. Existing impact response remains. |
| Mission Log cannot scroll | All history rows are retained in a scroll panel, with mouse-wheel, keyboard and gamepad support. It opens at the newest entries. |
| User-city mission frequency control | New **Mission frequency** slider: **0.25x–4x**, in 0.25x steps. It changes scheduling intervals and the remaining wait, without changing mission expiry. Saved per session. |
| Night lights flicker during flight | City window coverage uses cell-safe mip filtering; window selection uses an integer hash and stable building origin. Distant occupancy blends toward its area average while remaining masked to windows. Cooked city materials and mask sampling settings are rebuilt. |
| Capture cage triples crime | Removed the 3x crime weighting. Cage ownership still enables applicable recovery jobs. |
| Progress slider locked behind points | Progress is editable in User-city settings and can select any of four stages immediately. Equipment and valid mission-site requirements still apply. The manual choice survives saves. |
| User-city starting time mode | Starting or loading a User-city selects **Dynamic** time. A restored live clock hour and configured day/night durations are preserved. |
| Hold airplane brake to reverse | Once ground throttle reaches idle, continuing to hold the brake produces a slow reverse taxi. Release stops reverse; airborne flight cannot reverse. |
| Fleet unavailable after hangar visit | Leaving the hangar ensures the owned fleet exists, recalls it to available airport pads, and resets aircraft fuel, health and parked state. |

## Validation and installation

Validation receipt: [PlaytestOctober18Results.json](validation/PlaytestOctober18Results.json).
Scratch evidence, builds and regression logs: `Docs/scratchpad/playtest-2026-10-18/`.

**Completed:** Editor, Shipping and Development builds succeeded; all **192 distinct automated cases** have passing final results. The Shipping cook/package completed without shader errors. Both installed executables passed isolated 30-second headless startups.

Installed seven changed executable/container/manifest files into `C:/SimCopterRemake/SimCopter Remake v1.0.1/Windows`. Previous files are backed up and hash-verified under `Docs/scratchpad/playtest-2026-10-18/installed-backup`. All **205 protected city, save, configuration and optional radio files** were unchanged. Full installation hashes are in the validation receipt. This is a local update, not a published release.

The automated recovery fixture includes a blocking wall and a body beside a facade; it follows the medic through pickup, reboarding, hospital return and vehicle retirement. Additional tests exercise the EMS cap and explicit fourth dispatch, turbo descent at multiple time steps, thin-wall sweeps up to 400 meters, below-map on-foot recovery, reverse taxi, farewell save/restore, city-option persistence and legacy saves, and mission-log scrolling.

Offscreen D3D12 captures cover the menus, piers, hospital illumination and moon assets. These checks do **not** substitute for replaying the exact daytime horizon arc or nighttime flight route. No interactive gameplay was performed.

## Maintenance

- Ground-agent runtime save version is **13**; the appended float is the patient farewell timer. Older versions remain readable.
- Mission-actor runtime save version is **3**; appended fields are manual difficulty tier and mission frequency. Older versions retain score-based progression and 1x frequency until the user changes them.
- Window material generation is in `Tools/Unreal/CreateSimCopterMaterials.py`, shared atlas filtering in `CitySurfaceDetail.py`, and window-mask import settings in `BakeCityAtlas.py`. `UpgradeWindowFiltering.py` rebuilds the generated parent and rebinds the five city material instances.
- Before-source snapshots and a normalized source diff are retained in the scratch folder. The pre-existing Heven movement fixes were preserved.
