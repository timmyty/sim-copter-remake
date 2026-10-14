# Live lighting and city-tour overlay — October 14, 2026

Photo Mode froze the actor ticks that updated smoke, warehouse/hangar emissive
materials and interior fill lights. The exposure calculation could be correct
while those consumers retained an old daylight value. The shared exposure
subsystem now ticks presentation while paused, using weak material/light bindings
that retain each owner's multiplier, minimum and light-source/surface type.
Explicit time-of-day seeks invalidate the current-frame sample and refresh the
bindings immediately. Gameplay and animation remain paused.

All runtime writers of the shared `EmissiveNits` parameter were audited and routed
through this binding path: smoke, fire, pedestrian cards, particles, beacons,
dispatch/recovery markers, scoreboards, drive-in screens and taser effects.
The hangar's physical fill lights use equivalent weak bindings. Expired objects
are discarded; authored off states and multipliers survive a paused refresh.

Directional-light discovery is throttled, but the current key is selected from
the cached set by ground illuminance on every sample. This respects visibility
and `bAffectsWorld` and allows the moon to replace a sun below the horizon.

UE 5.8 eye adaptation reads the render family's world delta. The per-world view
extension supplies real delta when the paused family has zero world delta,
without changing its absolute game time or paused flag. It leaves an existing
nonzero presentation delta alone.

`SimCopter.City.ExposureRefreshWhilePaused` uses actual hangar and smoke materials
to cover paused and unpaused transitions, moon selection, authored scales and
off state, frozen game time, and the exposure view extension. Its D3D12 branch
waits across render frames before capturing a synthetic night scene. The capture
uses fixed exposure; it does not reproduce the user's exact city.

`SSimCopterControllerOverlay::GetPauseVisibility` excludes an active city tour
through `ASimCopterGameMode::IsCityIntroPlaying()`. Normal gameplay pause behavior
is unchanged. This small UI condition was reviewed in source.

The v1.0.15 publication checkout applies these scoped fixes to `daf151d`, retaining
the v1.0.14 cooked runtime content byte for byte. Other local development changes
are outside this release. Build, automation and packaged-startup evidence is in
[WindowsRelease115Results.json](../validation/WindowsRelease115Results.json).
Local release scripts, logs, source hashes and cleanup receipts are under
`Docs/scratchpad/release-v1.0.15/` in the development workspace.
