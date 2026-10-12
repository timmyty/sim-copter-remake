# Capture cage controls and skid deployment

User-requested remake addition, October 10, 2026. The earlier cage purchase had keyboard/controller support but no tool flap; its rendering also returned immediately when stowed.

The equipment bit now reveals a panel immediately below the megaphone. It reuses the original flap frame/grid, octagonal buttons, label/readout colors, selected-tool glow and HUD scaling. LOWER and RAISE are held actions; OPEN releases the floor. Controls never take keyboard focus, mouse holds survive the controller-input refresh, and switching tools or resetting input cancels the hold. The cage panel also participates in mission-marker avoidance.

Each fitted helicopter keeps compact steel/yellow emitter rails on its skids. The mount follows the actual airframe's bank, lowest point and width. Deployment extends bars from that mount toward the lowered rim; a two-sided grille closes automatically after capture. Reeling waits for the seal to finish. Empty searches leave the floor open. Raising to the top boards real occupants through the existing capacity/custody rules, then stows an empty cage. An open/release cooldown prevents immediate recapture.

The saved payload is unchanged. Formation and seal presentation are reconstructed from the existing deployed/open flags. Existing purchased cages gain the panel and fittings when their save loads.

The crime-family scheduler uses `3 * max(base crime weight, sum(base weights) / 7)` while any player-owned aircraft has a purchased cage. The sum is renormalized; the base city tuning remains unchanged. This adds crime to early cities while preserving all-zero/free-roam settings, existing difficulty-dependent crime types, concurrency limits and plane-only scheduling. The AI helper and temporary debug grants do not count as purchases. Selling the final cage restores the original distribution.

Validation evidence is recorded under `Docs/scratchpad/capture-cage/` and in `Docs/validation/CaptureCageResults.json`. No foreground gameplay session is required by this update; automated logic checks and offscreen renders cover the feature, with interactive control feel left to the player.

## Vehicle recovery and mission waypoints — October 11, 2026

The capture cage replaces the tow clamp. Its automatic pickup accepts stalled vehicles,
fleeing cars, temporarily tasered vehicles, and capsized hulls after their riders have been
rescued. One vehicle or hull occupies the cage; people retain their four-person capacity.
The floor closes automatically, the lower rim expands around the rendered cargo bounds,
and occupied cages stay deployed when raised. Vehicle routes survive release, while
caged getaway cars cannot advance their escape behavior. Both tasers now disable cars
for 16 seconds instead of eight; caged cars remain held when that timer expires.

Take cars to a junkyard and boats to a harbor. Slow down, lower the cargo into the lit
ring, then use **OPEN / G / D-pad left**. Acceptance requires the cage to be sealed,
cargo within the ring's 1.8 m horizontal radius, cargo height near the platform, and
cargo speed at most 2 m/s. Crossing the marker alone does not deliver player cargo.
The helper uses the same cage and acceptance rules and performs its own release.
Wrong-site or out-of-range opening releases cargo without completing recovery.

Junkyards and harbors have small permanent minimap markers (J and H). Their radius
grows from three to five raster pixels during relevant recovery work; marker size
stays constant across map zoom levels. Carrying a vehicle adds a bearing line and
destination label. Only active destinations show the pulsing world-space light ring:
amber for junkyards, blue for harbors. Completed work extinguishes the ring and shrinks
the minimap marker. Sites are generated from suitable terrain in each city; inland
cities need no harbor. Auto-repair platforms and the clamp's purchase/control entry
are retired.

Legacy clamp ownership upgrades to the cage while preserving other equipment.
Saved clamp selections and suspended cargo load into the replacement cage without
changing the save payload format or the existing equipment-bit ordinals.

Captured passenger portraits show translucent prison bars, angled silver wrist cuffs
with ratchet housings, and three interlocking chain links. Faces remain visible.

Current validation artifacts are kept in `Docs/scratchpad/cage-towing/`.

Editor and Shipping wrapper builds passed. All 48 selected headless cases passed
(47 clean, one existing `HangarPurchaseParking` fixture with two world-context warnings;
the two taser visual cases are skipped by their own NullRHI guards). The separate D3D12
offscreen recovery test passed, with active/inactive image comparisons for both light
rings and a present/hidden comparison for vehicle cargo. Actual dashboard portraits were
rendered at 1x, 2x and 3x. Synthetic scene and portrait screenshots were inspected; this
does not verify interactive control feel or a live-city flight.

The offscreen fixture creates its directional lighting before the first effect-exposure
query: creating it afterward cached the lightless night fallback and made the ring
invisible in an otherwise daytime exposure. This was a preview setup issue. Use final
color captures for these materials; BaseColor alone does not show their final appearance.

The validated Shipping executable is installed in the local v1.0.1 game folder, with
the previous executable backed up and both hashes verified. No cooked assets changed.
See [validation results](validation/CageRecoveryResults.json) and the installation
receipt under `Docs/scratchpad/cage-towing/`. No interactive playtest, publication sync
or push was performed.

## Short drops into the junkyard — October 11, 2026

Junkyard car delivery now accepts **OPEN** when most of the car is over the ring
and its body bottom is up to 3 m above the platform. The old height rule required
lowering almost onto the platform. A 9-by-9 grid samples the car's rotated body
footprint against the existing 1.8 m ring; more than half must lie inside. The
existing 2 m/s speed limit, sealed cargo requirement, and active-site checks
remain. Boats and police roof deliveries retain their existing rules.

An accepted drop transfers the car onto the junkyard lift and completes the
stalled-vehicle or burglar mission through the existing recovery path. This does
not introduce a new ballistic fall or change saves. The actual cage regression
now rejects small edge overlap and excessive speed, accepts a rotated elevated
burglar drop, and checks that repeated release cannot award the mission twice.
It failed before the acceptance change and passes afterward. Evidence is in
`Docs/scratchpad/equipment-preview/` and
[the combined results](validation/EquipmentPreviewAndAirportRecoveryResults.json).

## Police-roof drops and original mission priority — October 11, 2026

Police roofs now accept the same short-drop height as junkyards, over the station's
actual roof footprint. Most of the rotated car must overlap the roof, the cage must
be sealed, and cargo speed must be at most 2 m/s. This supersedes the narrow police
acceptance rule described above. Both stalled and burglar cars complete their mission
once when accepted.

Original dispatch and SOS rescues take priority over optional vehicle/hull recovery.
See [mission priority details](RadioVolumeAndStalledVehicles.md#mission-priority-and-police-roof-drops--october-11-2026)
for the reduced breakdown rate, scheduler correction, helper priorities and repeat SOS
behavior, including protection for survivors and attached cargo.

## Harbor short drops and police-station boats — October 12, 2026

Boats now use their visible rotated hull footprint for delivery, with the same short
drop allowance as cars: most of the hull must overlap the harbor ring or police roof,
its bottom must be no more than 3 m above the marker/roof, and cargo speed must be at
most 2 m/s. Lower the sealed cage, slow down and use **OPEN / G / D-pad left**.
This replaces the narrow boat-pivot height rule described earlier.

Police-station roofs also accept recovered boats. An accepted drop seats the upright
hull on the receiving surface and completes its recovery mission once. Actual hull
geometry, harbor and police completion, rejected approaches, and duplicate-credit
protection are covered by the headless cage test. See
[megaphone and boat recovery fixes](MegaphoneAndBoatRecoveryFixes.md).
