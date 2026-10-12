# Megaphone effects and boat recovery

October 12, 2026

**Stop Criminal** now reaches the original pedestrian reaction and the vehicle stop
handler. Boats can be delivered to either a harbor ring or a police-station roof.

## Original megaphone behavior

The accepted interaction stored the message in a separate C++ field, but the shipped
`people.df` BHAV 901 reads person attribute 13 (`person+0x15a`). The original
`FUN_004c1050` writes that slot, along with attribute 12 (`+0x158`). Both writes are
now mirrored into the VM attributes. Old saved reactions repair those attributes
when loaded.

BHAV 901 checks the spotlight location and player tile. Its Stop Criminal branch
faces the selection, plays `Whoa`, and calls BHAV 1104 (`Idle-80`). This is a brief
reaction, not an automatic arrest. Messages 2 and 3 again reach their original
agitation-reduction and flee branch as well. Broadcast range and reaction acceptance
still apply.

The vehicle handler previously rejected every message except Report Traffic. It now
accepts Stop Criminal for eligible ordinary cars and spotlight-marked speeders on
safe road tiles. Service vehicles, intersections, jams and disabled vehicles retain
their exclusion rules. Ordinary cars stop briefly and resume; speeders keep waiting
while the player or police remain nearby and resume if abandoned. The command does
not award a capture. The burglar's `CARROBBR` override still rejects a self-halt while
cruising/fleeing; a police stop retains its separate caller and spotlight gates.

Ground truth is the local `.ghidra-exports/` `decompiled` fields for `004c1050`,
`0049d7e0`, `0049df60`, `0049e0c0`, `0049ed20`, `0049b930`, and `004681f0`, plus
`Reference/SimCopterOriginalGame/x/people.df` BHAVs 901 and 1104. The port uses the
existing vehicle braking rather than introducing a separate driving model.

Agent save version 12 appends the temporary vehicle-stop timer and continues reading
versions 1–11. Tests exercise reconstructed version-8, version-10 and version-11
payloads, pending pedestrian reactions and active ordinary-car holds.

## Harbor and police-station delivery

The old harbor test used the cage pivot and a narrow vertical window, so a visible
hull over the ring could fail even though it looked ready to drop. Acceptance now
uses the hull's rotated local bounds at the current cargo location, including input
between cargo simulation ticks. More than half the sampled hull footprint must be
inside the harbor's 1.8 m radius or the police roof footprint. The hull bottom may be
up to 3 m above the receiving marker/roof, with 70 cm tolerance below it. Cargo speed
must remain at most 2 m/s.

Lower the sealed cage over either destination, slow down, then use **OPEN / G /
D-pad left**. An accepted delivery rights the hull, seats its bottom on the harbor
cradle or police roof, completes the hull recovery mission and awards it once.
Crossing a marker alone does not deliver cargo. Survivors must still be rescued
before towing their hull. Wrong sites, small overlap, excessive height and fast
flybys remain rejected.

Police-station boat delivery is the requested extension; it is not claimed as an
original-game feature. Cargo instructions now mention both boat destinations.

## Validation

The new regression fixtures failed before the production changes: 19 megaphone
assertions and 9 boat-delivery assertions. They passed after the fixes. The cage
fixture uses the actual `CAPBOAT1` mesh at a diagonal heading, tests short drops and
rejected approaches, and checks mission completion, single credit and roof seating.
The megaphone fixture executes the actual shipped BHAV data through the helicopter
broadcast path and checks vehicle stop/resume behavior.

The final related headless run passed all 61 cases (60 clean, one existing
`TunnelTransit` fixture with world-context teardown warnings). Render-only checks
that guard against NullRHI did not exercise rendering. The first combined run found
an outdated synthetic version-8 save fixture; both legacy taser fixtures were updated
to strip the new version-12 field, including the invalid-duration offset, and passed
the final run.

Build, source-hash and installation evidence is recorded in
[validation results](validation/MegaphoneAndBoatRecoveryResults.json) and
`Docs/scratchpad/megaphone-harbor/`. No interactive gameplay or listening check was
performed, and no cooked assets or publication checkout were changed.
