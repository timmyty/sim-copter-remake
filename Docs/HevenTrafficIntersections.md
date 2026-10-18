# Custom-map intersection traffic — October 18, 2026

The reported area is beside Heven 2026's airport block `(27,45)`, with the
transport plane on pad 11. The road graph was connected. Individual route
tests passed, but a 62-car, 90-second simulation reproduced the pileup:
14 cars remained stalled at a 0.025-second step and seven at a 0.1-second step.
Ambient recycling was disabled so disappearing cars could not hide the failure.

Two causes were isolated:

- `ApplyAircraftRoadBlocking` projected the car's displayed yaw toward an
  aircraft. Displayed yaw lags a turn, and an aircraft stop prevents movement
  from updating that yaw. Cars on the adjacent road could therefore wait
  forever for a plane that was outside their intended path. The probe now uses
  the active movement target, including guidance, avoidance and the reached
  guidance-point fallback. Its look-ahead ends at that waypoint rather than
  extending through the next turn. Actual movement still checks aircraft
  collision immediately before committing each step.
- Lane guidance reconstructed an endpoint that omitted route corner adjustments.
  Traffic displaced by other cars could oscillate just outside the real
  waypoint's arrival radius. After correcting aircraft detection, one such car
  remained stuck about 97 cm from its waypoint. Guidance now converges on the
  already assigned route target, including its corner or tunnel adjustments.

These correct the remake's movement/probe integration; they do not change the
original `FUN_0049ee30` following rules, intentional mission stops or map data.
The existing congestion spawn checks, car-only recovery and ambient recycling
remain as additional safeguards.

Regression coverage in `SimCopterTrafficFlowTests.cpp` includes stale mesh yaw
and velocity, a real aircraft obstruction with misleading yaw, guidance and
avoidance priority, waypoint-limited probes, consecutive turns, the original
Heven neighborhood with its plane, and dense traffic at two update rates with
and without road-height following. User-map route checks can be enumerated with
`-SimTrafficCities=<directory>`; maps whose airport has no neighboring roads use
their nearest connected road neighborhood. Heven cases are registered only
when its user-supplied map is available.

Evidence, before-edit snapshots, failed baselines, build logs and exported test
reports are in `Docs/scratchpad/heven-traffic-2026-10-15/`. The scratch folder
retains the date the investigation began. Final counts and installation hashes
are recorded in `Docs/validation/HevenTrafficIntersectionsResults.json`.

Editor, Shipping and Development builds succeeded. All 57 selected regression
cases passed, including route checks on 17 installed user maps. The final
62-car Heven simulation had no stalled cars in all four combinations of update
rate and ground following, with 6,500 or 6,571 route arrivals per run. Aircraft
collision, tunnel, dispatch, crime, accident and vehicle-taser regressions passed.

Both executables were installed into the existing local game folder with
verified backups and SHA-256 hashes. All 184 checked map/save/configuration files
were unchanged. Both configurations survived separate 30-second headless
startups using isolated profiles. Development reached the main menu; its
CelestialVault missing-star-material diagnostic also exists in the v1.0.19
baseline. Apart from the three implementation files and the expanded traffic
test file, source text matches that published release.

Validation uses headless Unreal simulation and packaged startup. It does not
claim an interactive replay of the user's exact session or a rendered city
playtest. No map, save format, cooked asset or publication checkout change is
required.
