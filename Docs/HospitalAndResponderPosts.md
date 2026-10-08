# Hospital entrances, responder spacing and vehicle impacts

Requested remake changes, 2026-10-05.

## Responder placement

The roof spawner always started at candidate zero, ignored existing people when
tracing support, and fell back to the deck centre when clearance failed. Every
worker could therefore receive the same position. It now searches 48 supported
positions, requires at least 90 cm between people on the same floor, avoids
aircraft, and retries later when no safe position exists. Hospital and police
staffing share the same placement checks. Exactly coincident workers in older
saves are separated without moving normal workers approaching a helicopter.

Hospitals and police stations also have a ground entrance post, with a sign and
one persistent responder. The hospital's preferred location is in front of the
lower entrance wing; blocked or wet approaches try another accessible side.
Terrain, geometry, water and occupancy are checked. Roof and entrance staff are
counted independently, including a height check, and absent entrance workers
use the existing 40-second replacement delay. Transient entrance caches/signs
rebuild on load; existing responder serialization carries their post and state.

Ground medics use the existing state-5 hospital behavior to unload nearby landed
helicopters. Patient reservations cannot cross floors. Walking a carried patient
up to the entrance, or setting one down within reach of its medic, also admits
the patient through the existing idempotent mission accounting. Admission checks
height, distance, a living available medic and an unobstructed handoff. It releases
the pilot's carry slot and credits each patient once.

## Hospital presentation

HO209 has dedicated procedural materials: ivory panel cladding, teal window
glass/trim, a dark plinth and double entrance doors, a textured roof finish, and
an antialiased helipad ring/H. The H is analytic geometry in the material shader,
so it no longer inherits the legacy 32-pixel atlas cell's resolution. Original
GEO vertices, roof heights, triangles, helipad position, collision and beacon
faces are preserved. Other buildings retain their existing atlas materials.

`Tools/Unreal/CreateHospitalPresentation.py` reproducibly builds the three
hospital materials and the new impact audio assets under the already cooked
`/Game/Generated/CityAtlas` directory. Offline before/after previews under
`Docs/scratchpad/hospital-response/` use the original GEO mesh and approximate
the new materials in neutral Blender lighting. They are not Unreal screenshots.

## Helicopter/vehicle contact

An accepted dent event plays a newly synthesized 0.56-second metal crunch and
emits 14 short gold sparks at its contact point. Sound uses the game volume,
spatial attenuation, and slight pitch variation. The existing contact separation
and 0.65-second vehicle cooldown also gate the effects, preventing repeated
sound/sparks while resting against a car. Existing second-impact immobilization
and fourth-impact fire/explosion behavior remain unchanged. The particle pool is
bounded and uses the existing exposure and replay handling.

## Validation

Build/test/package evidence, baseline source copies, the source diff, audio,
and offline previews are in `Docs/scratchpad/hospital-response/`.
Editor and Shipping wrapper builds passed. The final automation report has
68 passing tests: 66 clean and two older mobility-warning fixtures
(`PlaneDeckRescue`, `SafePassengerLanding`), with no failures or skipped tests.
Coverage includes spacing, full-roof rejection, legacy stacked workers, ground
staff, carried/dropped admission and duplicate credit, same-floor selection,
impact sparks/cooldown/audio import, plus both actual Islandtown hospitals'
collision, material bindings and accessible entrances.

The first cook revealed disconnected ComponentMask inputs in the generated
materials. The generator now uses the correct unnamed pins and asserts every
connection. Installation rejects material compile failures even when Unreal's
overall packaging command reports success.

The corrected cook/package passed with no material shader failures on SM5 or
SM6. IoStore inspection confirmed all three hospital materials, impact sound
(including its audio payload) and attenuation asset. All 870 NonUFS manifest
files exist. Nine changed installation files were copied and SHA-256 verified;
45 local city/save paths and all 21 custom radio tracks were preserved.
The complete prior installation is retained at
`C:\SimCopterRemake\SimCopter Remake v1.0.1\Windows-before-hospital-response-2026-10-05`.
Exact file hashes and paths are in the evidence folder's `installation.json`.
The cook still logs the existing editor map's stale `S:` drive city reference;
actual city loading and both hospital placements were exercised by automation.

Interactive gameplay, final Unreal lighting and listening still need a live
check; no foreground game session was launched (per repository workflow).

Manual checks: land at a hospital roof and unload several patients; observe
separated medics walking to the cabin. Visit the signed ground entrance with a
landed helicopter, then with a patient carried on foot. Confirm each admission
once. Collect a ground police officer and check that roof officers remain
available. Compare the facade and H from low altitude. Strike a car, keep contact,
then separate and hit again: one crunch/burst per accepted impact.
