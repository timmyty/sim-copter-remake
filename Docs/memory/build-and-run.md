# Building the C++ — always use RebuildUnrealCpp.bat

To compile, run `RebuildUnrealCpp.bat` at the repo root. Do **not** invoke
`C:\GameDev\UE_5.8\Engine\Build\BatchFiles\Build.bat` directly.

**Why:** the wrapper pins the engine root, the project file, the
`SimCopterRemakeEditor Win64 Development` target and — the part that matters —
`-NoLiveCoding`. A Live Coding session holding `UnrealEditor-SimCopterRemake.dll` makes a raw
Build.bat run fail to link, or silently hot-patch instead of relinking, so you end up testing
stale code. James rejected a direct Build.bat invocation and asked for the wrapper.

**How to run it non-interactively:** the script ends in `pause`, so feed it empty stdin or it
hangs waiting for a keypress. From PowerShell:

```powershell
cmd /c "S:\Repos\sim-copter-remake\RebuildUnrealCpp.bat < nul" 2>&1 | Select-Object -Last 35
```

(PowerShell 5.1 reserves bare `<`, hence the redirect living inside the `cmd /c` string.)
A clean build is ~60 s and ends with `Result: Succeeded`.

Launching the built game to verify a change: see `simcopter-ingame-verification` notes —
`-game -windowed`, then drive and screenshot the Slate UI from PowerShell.
Long build logs belong in `Docs/scratchpad/` (see `agent-workspace-conventions.md`).

## Optional game configurations — October 11, 2026

The wrapper accepts `Shipping` for the playable game or `Development` for GPU
diagnostics; omitting the argument still builds the Editor. Only the standalone
Development branch uses `-NoHotReload` instead of `-NoLiveCoding`: the installed
UE 5.8.3 Development libraries require the default reload-compatible class
registration layout. Turning Live Coding compilation off only in game modules
causes a pre-log access violation despite a successful link. The corrected build
passed packaged D3D12 startup with DRED/Aftermath. See
[the verified ABI trap](../BuildTroubleshooting.md#standalone-development-registration-crash-2026-10-11)
and [GPU diagnostics](../GpuCrashAndSprint.md).

## Copying changes into the publication checkout — October 14, 2026

Refresh the modification times of copied source and header files before building.
`shutil.copy2` preserves the development file's time, which can be older than an
existing publication object file. UnrealBuildTool can then report success while
reusing an object compiled from different source contents. In v1.0.16 preparation
this skipped six changed translation units, leaving the cheat table, cheat guide,
cockpit and beacon behavior stale; automation exposed the mismatch.

Use `copyfile` for changed source, or explicitly touch each copied source/header.
Verify the build log compiles every changed C++ translation unit in each target.
The release source hashes must still match the validated development snapshot.
Refreshing times and rebuilding all three targets resolves this without changing
gameplay code or disabling tests.
