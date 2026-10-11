# Traffic flow and flight pacing — October 11, 2026

User-requested changes to intersection behavior, automatic mission helicopters, and normal Shift flight.

## Traffic

The default `Original` traffic branch previously returned without running blockage recovery. Ordinary following also lacked the same-direction/height filters already used by mission queues, retained a nonzero creep at bumper distance, and considered separation jitter to be forward progress. These mechanisms can sustain intersection clusters.

Both AI modes now use direction-aware following with a full stop at body clearance. Traffic on another deck does not participate in following or overlap separation. Stopped vehicles derive their direction from their route target before falling back to visual yaw.

At junction decisions, ambient cars evaluate connected exits and the next segment, counting directed traffic and already planned entries. Held vehicles cost more than moving vehicles, dead ends are discouraged, and the opposite lane does not count. A clear existing choice is retained; cars still turn around at genuine dead ends. Mission-assigned routes remain owned by their mission.

After four seconds without moving 0.35 road tiles from the progress anchor, a car with an ordinary car blocker can phase through other eligible cars for 3.2 seconds. It follows the same road target; recovery does not reverse, teleport sideways, or disable collision channels. A six-second cooldown follows the phase. Aircraft blocking, mission jam chains, red lights, breakdowns, taser stops, fires, police stop orders, and mission-owned cars are excluded on both sides of a phased pair. New holds cancel an active recovery.

This is an explicit gameplay divergence from the original's anti-deadlock handling. The direction filter and stopping behavior agree with the documented `FUN_0049ee30` evidence in [traffic jam notes](memory/simcopter-traffic-jam-queue.md).

## Helper and player flight

Automatic helper eligibility comes from the current city's graduation state. A persistent unlock from a previous city or an older save cannot authorize spawning or dispatch in an unfinished new city. Normal city advancement resets the career helper flag as well. Career warps select a new city and therefore require its graduation again. Manual AIR: AUTO and AIR: MISSION obey the same gate.

The mission helper cruises at 600 cm/s, reduced from its independent 1,400 cm/s autopilot speed. That previous speed was not caused by `superpowermultiply`. Remote taser pursuit and unattended pilot recovery retain their separately specified speeds.

Without the cheat, either Shift key increases player helicopter horizontal travel by 30% and adds a steeper displayed pitch proportional to forward speed (30% more at steady cruise). This also works while easy-flight controls return toward level during coasting. Releasing Shift restores normal travel and pitch. The multiplier is applied once to the final horizontal velocity, leaving the underlying speed state, vertical motion, and parked/dead aircraft unchanged. With `superpowermultiply`, Shift continues to select the existing x10 turbo and climb behavior; the normal boost never stacks with it. Input-derived boost state is not persisted in saves and is cleared during loading.

## Verification

Automated coverage includes same-lane, opposing and crossing traffic; bridge separation; recovery despite jitter; phase expiry and protected stops; congestion-aware junctions and dead ends; a four-car crossing at two update rates; ordinary Shift speed/tilt and release at two update rates in both flight models; unchanged turbo; actual helper movement; and graduation/reentry eligibility.

Build and test evidence is kept in `Docs/scratchpad/traffic-flow/`. Final results are recorded in `Docs/validation/TrafficFlowAndFlightPacingResults.json`. No interactive playtest is claimed.

The final Editor build passed and all 48 selected headless tests passed: 47 clean passes and one pass with the existing four `TunnelTransit` synthetic-world teardown warnings. The first run caught two coasting-tilt assertions in the new boost test; deriving the extra tilt from forward speed corrected those while preserving the 30% velocity ratio and unchanged cheat behavior. The final run includes original flight-model, career transition, turbo, aircraft obstruction, accident, crime, tunnel, and save regressions as well as the new traffic and pacing scenarios.

The Shipping build also passed. The installed executable under `C:/SimCopterRemake/SimCopter Remake v1.0.1/Windows` was replaced only after the game closed and all validation gates passed. Its SHA-256 matches the built executable: `86BFC36D519A23A28CA10FD29CFCAF0B5AA24A8B5F3CAF060C09DBFBAED3C399`. The previous executable is preserved in `Docs/scratchpad/traffic-flow/installed-backup/`. Assets, saves, and settings were not replaced. The separate publication checkout was not synchronized or pushed.
