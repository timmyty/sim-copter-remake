# Hospital ground medics and entrance sign

October 8, 2026.

The hospital entrance no longer creates the text sign, panel, or pole. Its service
point and patient admission remain available. Police entrance signs remain in use.

Ground medics retain their compact idle patrol (280 cm post half-extent multiplied
by the existing 0.45 idle fraction, inset by the body radius). Purposeful approaches
can use the additional 120 cm that already defines helicopter detection. They can
therefore reach an aircraft near the entrance instead of stopping at an invisible
boundary before touching it. Building collision and normal boarding rules still
apply, including a waiting medical call, available seats and landing clearance;
patients already in the cabin retain priority for hospital unloading.

Entrance workers keep their hospital identity while approaching outside the small
patient admission circle. Staffing identifies the assigned post, avoiding extra
workers when an existing medic walks beyond the old 300 cm search circle. Nearby
legacy workers have their post restored. A ground worker's post no longer becomes
abandoned after a large displacement. Boarding still explicitly releases it, so a
medic can travel with the helicopter and deploy elsewhere.

The original state-5 program remains in charge: BHAV 801 selects hospital service,
263 unloads patients, and its no-patient arm calls 269 to board. Original executable
`FUN_004ca700` selects the player's helicopter when no starting emergency vehicle
is assigned. These changes adapt the remake's entrance confinement; they do not
replace those programs or change the save format.

## Validation evidence

The new `SimCopter.ServicePosts.GroundMedicPatrolAndBoarding` actor test reproduced
the original failures before the fix: the entrance sign existed, a displaced
worker abandoned its post, and the original medic program stopped at approximately
266 cm on each axis without boarding the nearby helicopter after two simulated
minutes. Ordinary idle walking remained within 117 cm on each axis.

The fixture checks a minute of idle walking, displacement, saved-post restoration,
staffing during an approach, distant and airborne rejection, original-program
boarding, real seat consumption, and travel after takeoff. Evidence and source
backups are in `Docs/scratchpad/hospital-ground-medics/`.

Editor Win64 Development compiled successfully through `RebuildUnrealCpp.bat`.
All 81 targeted automation tests passed: 79 clean and two with existing fixture
warnings (`PlaneDeckRescue` and `SafePassengerLanding`, also present in the prior
witness-photo validation). No failed or unrun tests. The corrected medic's idle
extent was 117.9 cm; it boarded through the original program, consumed one seat,
and followed the aircraft after takeoff. The suite also exercised the actual
Islandtown hospital geometry, rooftop crew, medical handoffs, passengers, behavior
VM, and collision rules.

Windows Shipping also compiled successfully through `RebuildUnrealCpp.bat Shipping`.
Existing Unreal API deprecation warnings remain. A compact test manifest is in
`Docs/validation/HospitalGroundMedicsResults.json`.

## Installed update

Installed the validated Shipping executable and PDB into
`C:\SimCopterRemake\SimCopter Remake v1.0.1\Windows` and updated their manifest
timestamps. SHA-256 verification passed for both installed files and the prior
binaries/manifests backed up in
`Docs/scratchpad/hospital-ground-medics/before-install-20261008-121846/`.
`installation.json` records the exact hashes. No content recook was needed.
Saves, cities, settings and radio tracks were outside the replacement list.
The seven changed C++ source/header files also match the local publication checkout.

No interactive city gameplay check was performed. To check visually, visit the
hospital front entrance, observe the small idle patrol, then approach in a landed
helicopter with a free seat while a medical patient awaits pickup. The medic should
walk into the cabin. Return with a patient to check the ordinary hospital handoff.
