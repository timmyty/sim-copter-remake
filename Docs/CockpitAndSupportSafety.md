# Cockpit, career aircraft, radio and air-support safety

October 10, 2026.

The original wiring panel is `bmp/dash2.bmp` (84 by 291 pixels). The original
cockpit assembly is recorded in `FUN_00412440`. The restored panel occupies half
its original share of the 640-by-480 screen. Its height reaches from the existing
top controls to behind the bottom instruments; its narrower width is calculated
from that area so the wiring forms one continuous cockpit side. A small arrow
retracts it to a six-page-pixel edge strip and
restores it on the next click. The choice survives HUD rebuilds and restarts.
Continuous side and bottom trim connects the existing map, passenger window,
instrument panel and tool controls; those controls retain their current anchors.

Career warps now resolve the possessed or explicitly remembered parked aircraft.
An on-foot city start remembers its starter helicopter. If neither link exists,
the fallback prefers a helicopter over the airport's spare Transport plane,
instead of selecting whichever actor name sorts first. A deliberately selected
plane still remains the player's current aircraft.

Each city starts with radio power on and restores the saved station by call sign.
A profile without a station starts on KROC (Rock). A saved nonzero volume remains;
the zero-volume OFF position resets to the default audible volume at level start.
Turning the radio off during a level still works. The intro soundtrack and
on-foot audio gate retain their existing behavior.

Air support uses the full visible aircraft envelope for obstacle avoidance,
including other aircraft. It finishes horizontal approaches before descending,
uses the actual cabin-door offset and aircraft underside to set hover height,
and checks passenger exit and delivery positions before releasing a passenger.
Transport destinations use their roof when available. Helper spawn placement
checks for surrounding geometry. Helper flight never enters the player-caused
car-impact path. Its medical patients are stabilized during the assigned rescue
and hospital handoff.

Rescue responsibility is recorded on each real passenger. Defensive helper
casualty handling removes only that victim from the unresolved workload without
charging a player penalty or awarding a fictional delivery. Other victims in a
shared mission remain accountable normally. Ground-agent runtime saves are now
version 8; versions 1–7 remain readable.

Cars retain their existing anticipatory aircraft stop rule and now check their
actual swept body bounds immediately before movement and separation nudges.
This catches turns, large frame steps and helicopters landing after the earlier
traffic-system probe. Cars resume their existing routes when the obstruction
clears.

## Validation and installation

- Editor and Windows Shipping builds succeeded through `RebuildUnrealCpp.bat`.
- All 72 selected headless tests passed. Two existing synthetic-world fixtures
  (`PlaneDeckRescue` and `SafePassengerLanding`) reported attachment/context warnings.
- The helper test flies the full aircraft past a blocking building, picks up a
  critically injured real patient, and delivers that same person alive onto a
  hospital roof within two simulated minutes: one delivery, zero casualties.
- Forced helper/car contact leaves the car intact and the player's cash and score
  unchanged. Responsibility survives a ground-agent save/restore. Car tests cover
  stale traffic probes, turning movement, separation nudges and resuming after takeoff.
- Offscreen cockpit rendering passed for expanded/collapsed states at 1280x720,
  1920x1080 and 2560x1080. Images were inspected; control anchor positions are
  unchanged. The render fixture reports two optional flap-art fallback warnings
  (`FLAP-dispatch.BMP` and `FLAP-apache-missles-gun.BMP`).
  Its blank map and existing 100%-HUD overlap at 720p are fixture/layout properties,
  not changes to map content or control placement.
- The local v1.0.1 Windows installation received the Shipping executable and PDB,
  with verified backups, SHA-256 matching and updated manifest timestamps. Runtime
  artwork is reused; no asset recook was needed. Saves and custom media were untouched.
- Source changes were synchronized locally to the publication checkout, preserving
  unrelated work and its Git index. No commit or push was made.

There was no interactive gameplay or listening check. Machine-readable evidence:
[CockpitAndSupportSafetyResults.json](validation/CockpitAndSupportSafetyResults.json).
Build logs, complete test reports, six previews and installation/sync receipts are
in `Docs/scratchpad/cockpit-support/`.
