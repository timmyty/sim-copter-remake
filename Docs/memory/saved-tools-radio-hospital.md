# Saved attachments, radio navigation and Heven hospital investigation

## Attachment mismatch — October 14, 2026

Reported with a controller after loading: Water Bucket showed the rescue harness;
switching to Harness and back made Bucket work. Automatic pilot recovery's
`EndPilotRecovery` reset `WinchState` to its default Bucket while leaving
`bHarnessRopeEndSelected` true. The real winch and visible attachment then disagreed,
and the save preserved both values. `StepWinch` only reports an attachment swap;
lowering an already-selected bucket cannot repair that stale presentation flag.

The producer now clears the flag when resetting the recovery winch. Restoration
derives the cached display flag from the saved authoritative rope end, repairing
existing affected saves without changing their byte layout or the original winch
transition behavior. Original reference: `SimCopterWinch` and
`Docs/scratchpad/ghidra/heli_tools_models_decode_20260724.md`, section 3.

`SimCopter.RemoteRecovery.TaserExitAndRescue` exercises automatic solo recovery,
capture/restore, the actual controller tool-wheel bindings, lowering the bucket,
the visible original attachment meshes, legacy contradictory saves, and the
Harness -> Bucket workaround. Nine assertions failed before the production fix.

## Radio controller navigation

The radio inherited `SCompoundWidget` and handled playback shortcuts but never
connected its station/track lists to the controller menu host. Eight routed-input
assertions reproduced the failure. It now uses `SSimCopterNavigableMenu`, starts
selection at Stations, and supports A to enter a list, directional browsing, A to
play, and B to leave the list before closing. The footer describes this behavior.
The same focus/edit loop includes the native seek and volume sliders.

Spatial selection prefers controls in the same visual row/column before diagonal
fallbacks. Previously a short button diagonally below Stations could win over the
wide Tracks list immediately to its right. Controller regression coverage includes
real Slate D-pad and left-stick routing, track playback, volume, seeking, and B.
Seek assertions use a WAV with a nonzero duration so an outstanding asynchronous
FLAC decode cannot make a zero-duration assertion pass vacuously.

## Hospital report: unresolved, no gameplay change

The user was resting on a hospital rooftop pad with a patient and a medic in the
two-passenger helicopter after loading Heven (new)'s last Exit Save. The stuck
patient was no longer available when an exact-state Quick Save was requested.

Copied `SimCopter_U_ExitSave.sav` read-only into
`Docs/scratchpad/bucket-save-selection/heven-exit-save.sav` before investigating.
SHA-256: `9E64DDAB10CBD6AFD927657ED9FF971179C703FC2E26ADD4A16B70EC5F24E6C7`.
The snapshot contains aircraft model 4 and one onboard medic, but no onboard
patient. The saved medic's behavior stack is 801 -> 272 -> 261.

An isolated world restored aircraft, traffic, missions, the actual city file,
and the authored CityRender actor transform (-640, 10, -180). The nearest hospital
roof resolves to (-23640, -20190, 2147.198). The restored roof team removed a patient
from the full two-seat cabin with the onboard medic deliberately inactive, using
real hospital collision for the landing gate. Repeating with an actual casualty
from the save (health 76, not delivered, identity `SimCopterGroundAgent_2147474162`)
also succeeded. Earlier runs allowing the onboard medic to work succeeded too.
These tests do not reproduce the exact unavailable stuck state.

The diagnostic test source is retained as `SavedHospitalInvestigation.cpp` in the
scratch folder; it was removed from the normal medical test before final builds.
No user saves or hospital gameplay code were changed. If this recurs, a Quick Save
while the patient is still aboard on the pad is the next useful evidence.

## Validation

Build logs, baseline failures, headless reports, and offscreen radio captures live
in `Docs/scratchpad/bucket-save-selection/`. See
`Docs/validation/SavedToolsRadioHospitalResults.json` for final counts and installation
status. No physical controller or interactive flight test was performed.

Final Editor, Shipping and Development builds succeeded. All 47 focused headless
cases passed (one expected missing-save warning), and the radio offscreen case
passed with 1280x960 and 1280x720 captures inspected. After the user closed the
game, both local executables and their symbols were installed with verified
backups and SHA-256 hashes. Publication checkout unchanged. Concurrent Turbo UI
work in the shared source was preserved; its validation belongs to that task.
