# Energy taser and free remote aiming

User-requested Unreal remake changes, October 11, 2026.

The on-foot taser now has a graphite and ceramic body, inset blue-green circuitry,
and four hinged muzzle emitters. Hold RMB or LT to aim. A visible, active criminal
person or criminal vehicle within 18 metres and seven degrees of the sight opens the emitters and charges a
floating orb over 0.65 seconds. A low electrical hum follows the charge. The
appearance communicates readiness without displaying mode names.

LMB or RT releases the formed orb when the current sight is near a visible
criminal person or vehicle. The orb travels
from the weapon muzzle, steers toward its selected target, and stops on world
geometry, intervening people, or vehicle bodies. Release, flight and contact use
distinct synthesized positional sounds. No Apache weapon or damage path is used.

Contact stuns an uncollected pedestrian for 12 seconds or stops a criminal car
for sixteen seconds after the cage recovery update. Picking up the pedestrian still handcuffs them;
the timer cannot release a handcuffed prisoner. Health, identities and arrest
delivery credit are retained. Agent save version 11 adds the remaining pedestrian
stun timer and recovery flags and reads versions 1 through 10.

Losing sight of the criminal immediately removes the orb, closes the emitters
and stops the charge hum. Ordinary civilians and vehicles cannot charge or fire
the weapon. Lowering it also clears the charge; reacquiring a criminal requires
a new 0.65-second charge. A launched orb finishes independently of holstering.
This supersedes the original 30-second charge memory and three-second fade.

The helicopter remote sight now keeps a world-space aim controlled by mouse or
right stick. Entering remote mode, moving the helicopter, and handing pursuit
between a car and its burglar do not turn the sight. The full yaw range, pitch
limits, RMB cockpit cursor access, and manual firing controls remain.

Autopilot selects active criminal people, getaway cars whose event is still
active, or designated fleeing speeders. Generic fleeing traffic and stale pooled
getaway cars are excluded. With no target it holds position. With a target it
accelerates along the existing swept, full-airframe path to a nearby clear
position, capped at that airframe's normal maximum speed times the existing 1.3
sprint factor. It follows moving targets and can seek a clearer elevated or side
position around buildings. It does not warp to a pursuit position.

Implementation uses the existing cooked vertex-colour and particle materials and
runtime procedural geometry/PCM; there are no new assets or plugin dependencies.

Validation evidence is stored in `Docs/scratchpad/energy-taser/`.

## Initial validation and local installation

Editor and Shipping builds passed through `RebuildUnrealCpp.bat`. All 21 selected
headless cases passed with no test warnings or failures; two visual-only cases
are deliberate no-ops under NullRHI. Both real GPU offscreen cases also passed,
covering the redesigned weapon and the existing helicopter discharge effects.
The weapon capture measured 44,945 changed pixels during transformation, 1,900
blue-green pixels and 2,460 visible weapon pixels from the actual shoulder camera.
Idle, charged, fading, expired, flight and impact captures were inspected.

The first regression run exposed a missing base-travel multiplier in the sprint
cap and fixtures relying on the former automatic sight snap or non-production car
speeds. Those were corrected before the passing run. The visual fixture required
its sun to exist before the weapon's first exposure query, and a mean-error
comparison to tolerate shadow-edge sampling noise between otherwise identical
closed-weapon captures. All earlier diagnostic logs remain in the scratch folder.

The local Windows executable was backed up, replaced and SHA-256 verified on
October 11 at 1:25 PM CDT. Its hash is
`D89E334A98B67676E2F922611B68A0E3FEDACCD629D24A9999A21606E401AAC7`.
Only the executable changed; the existing cooked materials are reused.
No interactive gameplay or listening check was performed. Existing engine
deprecation, startup map-path and GPU-plugin messages remain in the logs outside
the successful tests. The publication checkout was not changed or pushed.

See [validation receipt](validation/EnergyTaserFreeAimResults.json).

## Criminal-only readiness update — October 11, 2026

The later Apache-aftermath regression uses actual possession, Apache exits and
the pilot's shoulder camera. Charging and firing on a criminal passed even before
the readiness change, including repeated exits after machine-gun and missile use;
an Apache-specific input failure was not reproduced. Retained charge and open
emitters after looking away failed before the change and pass afterward. Ordinary
traffic remains inactive; an active criminal vehicle powers the taser normally.
The updated GPU offscreen check uses an actual target and verifies both the powered
and immediately closed appearances. See [current validation receipt](validation/ApacheAftermathResults.json).
