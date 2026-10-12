# GPU crash investigation and L3 Sprint Mode — October 11, 2026

## GPU evidence

The 8:37 PM CDT crash (`UECC-Windows-E3FBCAF549F716D1D8102498F38A73CF_0001`)
occurred 230 seconds into the installed Shipping game, with `-nvaftermathall` on
the command line. NVIDIA Aftermath was enabled; DRED was not. The RTX 4090 was
using driver 617.42 and DirectX 12 / SM6.

Offline decoding with the engine's bundled NVIDIA SDK confirms a GPU memory
page fault: a compute shader tried to read GPU virtual address zero. The faulting
instruction is at offset `0x1a0` in an 80,896-byte shader. Its low-quality
fingerprint matches the three October 10 dumps investigated previously, which
used driver 617.14. This establishes a recurring signature across the driver
update, not a unique shader identity or a proven cause.

The new dump still has no event markers, named resource, shader hash, or shader
source mapping. `compute_01` is a generic decoder label. It does not establish
that the cage, a city, DLSS, or Lumen caused the crash. The failure is not reported
as out of memory. About 40 GB of system RAM remained available; this is not a
measurement of available GPU memory.

## Next diagnostic run

The existing `-nvaftermathall` launcher did request the additional NVIDIA
features, but a Shipping build cannot provide the same rendering context as a
Development build. UE 5.8's `RHIBreadcrumbs.h` compiles full breadcrumb scopes
only for Debug/Development or builds with GPU profiling enabled. The original
game target does not enable Shipping GPU profiling. Command-line flags cannot
restore compiled-out scopes.

`RebuildUnrealCpp.bat Development` builds the game target with normal Development
logging and breadcrumb support. The separate executable can run beside the
Shipping executable against the same installed cooked content. The diagnostic
launcher uses `-gpucrashdebugging -nvaftermathall`, enabling the engine's GPU
diagnostics, including DRED, as well as NVIDIA collection. It intentionally keeps
the saved graphics choices and does not change the driver or Windows timeout
settings.

Installed launcher: `C:/SimCopterRemake/SimCopter Remake v1.0.1/Windows/Launch-With-GPU-Diagnostics.cmd`.
The previous October 10 launcher now forwards here. Logs are written to
`%LOCALAPPDATA%/SimCopterRemake/Saved/Logs/GPU-Diagnostics.log`; crash artifacts
remain under the neighboring `Crashes` directory. The launcher selects the
installed production NVIDIA DLLs with `-slbinaries=production`.

Reproduce the ordinary flight using the diagnostic launcher and retain the
resulting game log and crash directory. If graphics isolation is needed, compare
repeated runs with only Graphics > Lighting > Lumen changed from Hardware to
Software. A single successful flight is not proof. Keep DLSS, resolution and
other settings fixed for that comparison. No crash fix is claimed until the
faulting pass or a repeatable A/B result identifies the cause.

See [Epic's GPU crash guidance](https://dev.epicgames.com/documentation/unreal-engine/dealing-with-a-gpu-crash-when-using-unreal-engine)
and [NVIDIA's dump inspector reference](https://docs.nvidia.com/nsight-aftermath/UserGuide/gpu-crash-dump-inspector.html)
for the distinction between a CPU notification stack, GPU markers, and mapped
shader evidence. Local evidence and decoder are in `Docs/scratchpad/gpu-crash-l3/`.

## Sprint controls

- **Click L3** to toggle Sprint Mode in a helicopter or on foot. A second click
  switches it off; holding the button does not repeatedly toggle it.
- Helicopters use the existing 30% horizontal boost and steeper tilt. Pilots use
  the existing doubled grounded run speed, run animation, and footsteps.
- **Hold R3 and click L3** to show or hide controller help. The Sprint Mode state
  and help shortcut remain visible when help is collapsed.
- Shift still works. Shift and L3 combine as one request and do not stack. The
  superpower cheat retains its existing turbo/dog behavior through either input.
- Boarding, exiting and restoring a save clear the controller sprint toggle.
  On-foot running respects modal input holds and does not boost normal falling.
  Tool/passenger menus cannot toggle helicopter sprint accidentally.
- Airplanes retain L3 help and their existing flight controls.

This is an explicit controller extension of the fork's existing Shift behavior;
there is no change to original fixed-point flight arithmetic or save formats.

## Validation and remaining limits

Editor, Shipping and standalone Development builds succeeded. All 44 focused
headless test results passed without test warnings, covering controller routing, flight,
pilot running, recovery and save behavior. The offscreen controller-help render
also passed (headless help validation checks icon files, not rendering); expanded
PlayStation and collapsed Xbox presentations were inspected.
The updated Shipping executable and matching symbols are installed with verified
hashes and backups. The normal game launcher still starts Shipping.

The separate diagnostic build reached the packaged main menu and exited with
code 0 in a hidden D3D12 smoke run, using an isolated copy of settings. Its log
confirms Development, DRED including breadcrumb context, Aftermath markers,
resource tracking, shader debugging and production Streamline binaries. It also
reports unavailable DLSS frame generation because Windows hardware scheduling
is disabled, plus a missing CelestialVault material in the existing cooked data.
Neither prevented startup; neither identifies the reported flight-crash cause.

The first diagnostic build hit a separate pre-log static-registration crash.
The installed Development engine libraries need their default reload-compatible
class layout; passing `-NoLiveCoding` changes that layout in game modules. The
wrapper now uses `-NoHotReload` for the standalone Development target and retains
`-NoLiveCoding` for Editor/Shipping. See [build troubleshooting](BuildTroubleshooting.md).

No interactive controller playtest or sustained city-flight reproduction was
performed. The original GPU fault remains unresolved. The improved capture is
the next investigative step, not a claimed rendering fix. Evidence and installed
hashes: [validation results](validation/GpuCrashAndSprintResults.json).

## Controller follow-up — October 11, 2026

Sprint Mode remains visible outside expanded help, as requested. Expanded help
now explains both sticks, triggers, camera adjustment, on-foot actions and remote
taser controls. Longer lists use two columns and were checked at 1280x720.

The updated helicopter layout uses **Y to exit**, **X for Tools / Dispatch**, and
**A to use the selected tool or confirm a menu choice**. RT remains climb/start;
A no longer climbs. On foot, Y and walking close both retain boarding. Remote
Taser is a Tools choice: X, point at it with the right stick, then A to activate;
A or RT fires after activation. Select Fly Helicopter through the same menu to
return. Passengers and Recall All remain menu choices. The menus own the stick
while open so selecting an option does not also move remote aim or fire a shot.
Wheel selection persists when the stick returns to center. R3 plus D-pad up/down
zooms the minimap, and left/right cycles its mission. L3 plus D-pad up toggles
mission markers; down toggles service vehicle markers. These map shortcuts appear
in expanded help and do not also operate tools or toggle Sprint Mode.

Crime penalties now stop while suspects are in custody, including stunned or
handcuffed people, carried prisoners and immobilized/caged burglar vehicles.
Police-station delivery remains necessary to finish and earn the reward. The
regression reproduced recurring crime warnings and score deductions for all four
crime families before the lifecycle fix. Actual prisoner state, including save
restore, is checked separately from the deterministic mission timer.
See [current controller guide](ControllerSupport.md) and
[follow-up evidence](validation/ExpandedControllerHelpResults.json).
