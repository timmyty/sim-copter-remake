# Hangar controller selection and deployed medical crew — October 14, 2026

## Reproduced failures

Selecting a catalog aircraft rebuilt every Slate control. The navigation host held a
weak reference to the removed button, while the hangar restored only keyboard focus
and equipment-page selection. The next controller move therefore started from the
first transaction control. The regression reproduced the lost highlight and failed
next-aircraft selection before the correction.

Two manually dropped medics on hospital tiles ran BHAV 801's hospital branch into
263. That branch searches only the cabin; with no patient aboard it runs 269 and
reboards, ignoring nearby ground patients. A separate real two-seat aircraft test
showed the first returning medic taking the second patient's seat before the slower
rescue reached the aircraft. The manual drop path also released only the selected
medic, leaving any cabin patient behind.

Ground truth is the shipped `people.df`: BHAV 801 records 8/9 choose cabin unloading
263 versus ground retrieval 262; BHAV 269 returns without rechecking ground patients;
BHAV 263 records 3/10 remove and tote a patient, followed by record 24's carry-away
sequence. The decoded graph is retained in
`Docs/scratchpad/apache-aftermath/medic-programs.txt`.

## Corrections

Catalog tab references are rebuilt and selection is restored by tab identity,
including when transaction availability changes. Activating an aircraft leaves its
tab selected for the next D-pad or stick movement. Equipment navigation keeps its
existing item/action behavior.

Manual medic deployment records the helicopter as the existing starting-vehicle
reference. On a safe landing the medic enters the ground-rescue branch directly,
even on hospital tiles. Returning crew recheck for available ground casualties
outside the VM tick. Cabin boarding reserves room for teammates' claimed or carried
patients. The existing bounded pathfinder and patient claims still own navigation.

Before a grounded medic leaves the cabin, they claim a patient using existing
wounded-first triage, remove that same patient and tote them. This happens after
clear-exit validation and before stepping outside handoff reach. After the medic's
short fall to the ground, BHAV 263 continues the carry-away and set-down sequence.
Each medic takes a different patient. Existing handoff methods update the seat
manifest and idempotent mission delivery; no extra patient or delivery is invented.
Airborne drops do not remove cabin patients. Helicopter starting references are
excluded from the ambulance-only set-down flag. No save layout changes are needed.

## Validation

`SimCopter.Hangar.CatalogControllerSelection` exercises A activation, the rebuilt
selection, D-pad next-aircraft browsing and controller navigation back to the previous
aircraft. `SimCopter.MedicalNavigation.MedicDeploymentPriorities` uses the original
behavior programs, actual Schweizer and Agusta models, real mission records and the
public passenger-drop action. It covers two medics with one/two ground patients,
hospital drop tiles, staggered two-seat returns, one/two cabin handoffs, subsequent
set-down, duplicate-delivery rejection and an airborne drop.

The existing moving-target path test was updated to advance ordinary frames through
the retry deadline and bounded search, rather than requiring success in one large
time-only tick. Its route and work-limit assertions remain.

Builds and complete release validation are recorded in
[v1.0.12 results](../validation/WindowsRelease112Results.json). Detailed baseline and
focused reports are under `Docs/scratchpad/hangar-medic-deployment/`. No interactive
gameplay or physical-controller check is claimed. The earlier unavailable Heven
hospital state remains a separate unresolved report.
