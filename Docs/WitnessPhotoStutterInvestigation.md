# Witness photo stutter investigation — October 12, 2026

Starting with v1.0.13, download the matching **Optional-Development** archive from
[GitHub Releases](https://github.com/timmyty/sim-copter-remake/releases) and extract
its contents inside the extracted game folder, beside the normal launcher.
The add-on requires the same-version full game and supplies both diagnostic launchers.

The later 9:50–9:57 PM flight has now been decoded separately: [progressive flight stutter](FlightStutterInvestigation.md). It shows large ground-agent CPU stalls near the end and led to a reproduced responder pathfinding fix. This does not establish the cause of the earlier photograph-related report below.

The user reported severe choppy frames after the robber photograph appeared in
the latest GPU-diagnostics run. The slowdown cleared after roughly 25–35 seconds
and **continued after the photograph disappeared**.

## Evidence and limits

The preserved `GPU-Diagnostics.log` is in
`Docs/scratchpad/witness-stutter-2026-10-12/`. It covers the Development process
from 1:01:14 to 1:06:17 PM CDT. Its command line enabled
`-gpucrashdebugging -nvaftermathall`; the log confirms DRED and all five Aftermath
features, including call-stack capture. The saved settings use 2560x1440,
DLSS Quality, Hardware Lumen and cinematic scalability (4).

- Terraton (`city14.sc2`) loaded at 1:04:45 PM.
- Robber event 3 spawned at grid (48,48) at 1:05:36.364 PM.
- The witness code waits 0.25 seconds before taking its snapshot. At
  1:05:36.620 PM the renderer initialized its final-color LUT; at
  1:05:36.626–.632 it reported **19 new graphics PSOs**, all missed by precaching.
  This is consistent with the photograph's first render. The log has no explicit
  photo event, so this association is inferred from the timing and code.
- The run continued and ended through Alt-F4 at 1:06:16.673 PM. It did not record
  a GPU timeout, device removal or fatal GPU crash.
- Three nonfatal UV-channel-data ensures occurred during an earlier city load
  at 1:03:55–56 PM, about 100 seconds before the robber. The similarly timed
  directories in `Saved/Crashes` contain **Ensure** reports, not GPU crash dumps.

The log has no continuous CPU/GPU frame timings. Its frame counter is modulo
1000, so sparse entries cannot establish FPS or the length of the slowdown.
The PSO messages record encounter times, not compilation durations. They do not
prove that pipeline creation accounts for the sustained stutter.

## Code and engine checks

`USimCopterWitnessBriefing` disables both `bCaptureEveryFrame` and
`bCaptureOnMovement`, then explicitly calls `CaptureScene` once per report.
The visible panel displays the resulting texture. It does not redraw the city.
The photograph normally displays for 18 simulation seconds, with a possible
extension to accompany queued witness speech.

Persistent rendering state is intentional: the October 8 reversal test showed
that removing it causes the physically lit photograph to clip almost entirely
to white. Do not remove that exposure fix speculatively.

The installed UE 5.8 source defaults scene captures to no Lumen GI/reflections
and `bUseRayTracingIfEnabled = false`. The witness component does not opt in.
Consequently the mere existence of a persistent capture is not evidence of a
second continuously updating Lumen scene. A single capture can still trigger
render resources, PSOs and other rendering work.

## Ranked hypotheses

1. First-use rendering work triggered by the photograph: supported by the PSO
   burst, but sustained duration is unmeasured. Compare frame and rendering-thread
   timings before the report, at capture and throughout the following minute.
2. Additional crash-diagnostic work amplifies a rendering stall: the recorded
   run enables DRED and all Aftermath features. Not measured or proven. Compare
   the same Development executable and graphics settings with and without those
   extra flags; retain the same performance trace channels in both runs.
3. Another CPU/gameplay or GPU workload overlaps the report: not excluded.
   Frame, CPU, GPU and log trace channels can distinguish it from the photo.

No gameplay/rendering change or performance fix is claimed from this evidence.

## Next capture

`Tools/Unreal/Launch-With-Performance-Diagnostics.cmd` is installed beside the
normal launcher in `C:/SimCopterRemake/SimCopter Remake v1.0.1/Windows`.
It runs the existing Development executable, preserves normal saved settings,
and records CPU, GPU, frame, bookmark and log channels to timestamped
`%LOCALAPPDATA%/SimCopterRemake/Saved/Profiling/Witness-Performance_*.utrace` files.
Its text log is `Saved/Logs/Performance-Diagnostics.log` in the same user folder.
At the user's request, it now also enables `-gpucrashdebugging -nvaftermathall`,
matching the GPU-diagnostics launcher's DRED and NVIDIA Aftermath collection.
Crash reports and GPU dumps, when the engine/driver can produce them, remain
under `%LOCALAPPDATA%/SimCopterRemake/Saved/Crashes`. The performance trace is
written during the run; a crash can omit the last buffered trace events.
Tracing and the additional crash diagnostics both have overhead.

Launch it, play until a robber photograph appears, continue for at least one
minute after the photograph, then exit normally. Note whether the stutter
recurs. The trace will retain actual frame durations and CPU/GPU work alongside
the mission log, while retaining crash diagnostics if the GPU failure recurs.
Crash diagnostics are on by default; no extra arguments are needed. Any future
comparison without those flags needs a separate explicit change. Keep resolution,
graphics settings, city and route fixed. A single non-reproduction is not proof.

The original GPU-crash launcher remains available for investigating the separate
unresolved crash. Each trace has a timestamp; collect only the runs needed for
the comparison because full CPU/GPU traces can grow during long sessions.

Validation results are recorded in
`Docs/scratchpad/witness-stutter-2026-10-12/validation.json`. The startup check
uses offscreen rendering and an isolated profile; it is not a city-stutter
reproduction or evidence of improved performance.

Before enabling the combined crash flags, the launcher reached the main menu with an isolated settings copy;
the original settings hash stayed unchanged. Its 50.5 MB timed-startup trace
decoded in headless Unreal Insights (analysis exit code 0), exporting 580,598
game-thread events, 207,203 GPU graphics events and 39,524 GPU compute events.
The log confirms DRED and additional Aftermath call-stack capture were absent.
Normal default Aftermath initialization remained enabled.

Validation caveats: both the timed-exit run and a subsequent `Quit` run logged
normal Unreal shutdown but returned launcher exit code **777003**, so these are
not clean exit-code passes. That exit-code discrepancy is unresolved. Insights
also reported memory-tag diagnostics and a few GPU timestamp ordering
corrections (at most 35 microseconds), while successfully exporting CPU/GPU
timings. No city playtest, stutter reproduction, C++ rebuild or gameplay fix was
performed. Preserve those limits when reporting this investigation.

## Combined performance and crash collection follow-up

The user requested crash evidence in the performance run too. Both the source and
installed performance launcher now include the same `-gpucrashdebugging
-nvaftermathall` flags as the existing GPU launcher, in addition to the existing
timing trace. The previous launchers were backed up, and the installed/source
SHA-256 hashes and required flags were checked. This command-line-only change
does not require a C++ rebuild. No crash was deliberately triggered and the
combined flags have not been exercised in a new runtime check. The startup and
trace decoding results above describe the preceding version.
