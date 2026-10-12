# UnrealBuildTool startup exception in an agent sandbox

The build must run with filesystem permission for Unreal's build logs and caches outside the
checkout. For a workspace-write agent session, invoke `RebuildUnrealCpp.bat` with the execution
tool's `sandbox_permissions: "require_escalated"` from the start, using its normal approval
mechanism. Continue to capture build output under `Docs/scratchpad/` and feed empty stdin to the
wrapper's trailing pause. This does not require running Windows as administrator.

## Verified cause (2026-09-08)

A restricted build exited with `-532462766` immediately after printing the UnrealBuildTool
command. The engine's bundled .NET 10 SDK successfully ran `--info`. Launching UnrealBuildTool
with `-Help` exposed the exception that the engine batch wrapper did not display:

```text
Unhandled exception. System.UnauthorizedAccessException: Access to the path
'C:\Users\james\AppData\Local\UnrealBuildTool\Trace-backup-2026.09.08-17.09.12.uba' is denied.
   at System.IO.FileSystem.DeleteFile(String fullPath)
   at EpicGames.Core.FileReference.Delete(FileReference location)
   at EpicGames.Core.Log.BackupLogFile(FileReference outputFile)
   at UnrealBuildTool.UnrealBuildTool.Main(String[] ArgumentsArray)
```

In the installed engine's `Engine/Source/Programs/UnrealBuildTool/UnrealBuildTool.cs`, `Main`
creates and rotates `Trace.uba` before entering its `try` block or parsing command-line options.
`Engine/Source/Programs/Shared/EpicGames.Build/Unreal.cs` resolves that directory through
`Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData)` for installed
Windows engines. Supplying `-Log` therefore cannot relocate this startup trace.

The same wrapper succeeds with the execution tool's elevated filesystem permission. The
compiler and SDK installation are healthy; changes to C++ merely prompt another build that
encounters the same restricted trace directory. Build logs also show writes to user-level
Unreal caches and `%ProgramData%\Epic\UnrealBuildAccelerator`.

Do not delete old traces, reinstall .NET, alter engine source, or disable the workspace sandbox
for this failure. Grant the build invocation its required access. An ordinary terminal build
already has that access and continues to use the wrapper normally.

## Standalone Development registration crash (2026-10-11)

`RebuildUnrealCpp.bat Development` builds a diagnostic game executable alongside
the Shipping target. With the installed UE 5.8.3 engine, do not pass
`-NoLiveCoding` to this standalone Development build: its precompiled engine
libraries retain the default `WITH_LIVE_CODING=1` and hence `WITH_RELOAD=1`.
Changing only the game/plugin modules to `WITH_RELOAD=0` changes
`FClassReloadVersionInfo` and compiled-in class-registration structure layouts.

The mismatched build linked successfully, then failed before logging in
`TDeferredRegistry<TRegistrationInfo<UClass,FClassReloadVersionInfo>>::AddRegistration`
while registering DLSS classes (write access violation at address `0x10`).
This was a build ABI mismatch, unrelated to the user's GPU flight crash.

The wrapper's Development game branch now passes `-NoHotReload` instead. It
disables hot-reload build mode while preserving the installed libraries' class
layout. Editor and Shipping branches keep `-NoLiveCoding`. Build with the editor
closed through the wrapper; do not alter the installed engine source. The rebuilt
diagnostic executable passed a packaged offscreen D3D12 main-menu startup and
clean exit with DRED and Aftermath enabled.

Ground truth: UnrealBuildTool `Configuration/Rules/TargetRules.cs` and
`Configuration/Descriptors/TargetDescriptor.cs`, `Core/Public/Misc/Build.h`, and
`CoreUObject/Public/UObject/UObjectBase.h`. Debug-event capture, successful build
log and startup log are under `Docs/scratchpad/gpu-crash-l3/`.
