# Hospital and police posts, hospital materials, vehicle contact

User-requested remake changes, 2026-10-05. See
[implementation and verification](../HospitalAndResponderPosts.md).

- `TrySpawnOriginalPersonAtTile` previously always started its roof spiral at zero,
  ignored people in its support trace, and used the roof centre on failure. The
  shared 48-candidate search now checks people/aircraft occupancy (90 cm same-floor
  separation), keeps the existing support/decorations checks, and fails/retries.
  `EnsureServiceRoofCrew` shares hospital/police staffing, separates exactly
  coincident legacy workers, and preserves the existing replacement cooldown.
- `SimCopterServicePosts.cpp` owns ground posts and signs for D1/D2. HO209's front
  wing faces -Y after the GEO axis conversion; try the front first, then accessible
  alternatives. Persistent post position/extent already serialize with people.
  Entrance caches/signs are transient and reset on load/city rebuild.
- Ground and roof medics share state 5/BHAV 801 -> 263. Both patient reservations
  and nearest-worker selection now check height. Carried/dropped patients at the
  entrance also use the existing pickup/delivery accounting, once, within a live
  medic's reach and unobstructed line of sight. Consume the pilot carry reference
  before retiring the admitted actor.
- A test patient must load a real `FPeopleBehaviorModel` and enable behavior:
  `SetMissionInjuredPose` only resets State to 6 in that case. Merely assigning
  `InitialPersonState=6` does not make `IsMedevacVictim()` true.
- Hospital object ID is **0x016**, XBLD **0xD1**, model **HO209**. Only its atlas39
  facade, atlas2/cell9 helipad and type15 roof sections receive new materials.
  Geometry/collision/beacons remain original. Material coordinates are local cm;
  the per-model mesh already includes OriginalMeshScale. The H/ring uses analytic
  UV masks, with derivative antialiasing.
- `Tools/Unreal/CreateHospitalPresentation.py` bakes the three materials and
  generated sound/attenuation assets in the already AlwaysCook CityAtlas folder.
  **ComponentMask's input name is empty**, not `Input`, in MaterialEditingLibrary.
  Assert every connection. NullRHI asset saves do not prove shader compilation:
  inspect cook logs for `Failed to compile Material` and null ShaderMaps before
  accepting a package, even if BuildCookRun reports success.
- Accepted helicopter/car impacts add a 0.56 s synthesized metal cue and 14 short
  sparks, using the existing 0.65 s damage cooldown. Shared particle-pool origins
  must be distinct to preserve individual lifetime settings. No damage thresholds
  changed.

Evidence: `Docs/scratchpad/hospital-response/`. Editor and Shipping builds passed;
68 targeted tests passed (66 clean, two existing fixture mobility warning groups).
Offline Blender previews approximate materials on the original GEO and are not
Unreal screenshots. No live gameplay or listening check was run.

Corrected shaders compiled for SM5/SM6 and the full package passed. IoStore
contains all five new assets and the sound payload; 870 NonUFS files verified.
Installed nine changed files with SHA-256 verification and a complete prior-build
backup at `Windows-before-hospital-response-2026-10-05` beside `Windows`.
Preserved 45 city/save paths and the 21-song custom radio library.
