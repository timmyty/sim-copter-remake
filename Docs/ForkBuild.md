# Building the gameplay fork

This is an Unreal Engine 5.8 source fork. The October 8, 2026 validation uses
UE 5.8.3 on Windows. It includes all 21 KINV music WAVs, both prepared drive-in MP4 videos, and custom
artwork/audio through Git LFS. To play without building, download the Windows ZIP from
the [latest release](https://github.com/timmyty/sim-copter-remake/releases/latest), extract
it completely, and run `SimCopterRemake.exe`. The instructions below are for source builds.

## Checkout and dependencies

1. Install Git LFS, then clone the fork and run `git lfs pull`. A GitHub source ZIP
   may contain LFS pointers instead of playable assets; use a Git/LFS checkout.
2. Install UE 5.8 and its supported Visual Studio C++ toolchain. The Windows wrapper
   checks `C:\GameDev\UE_5.8`, then `%ProgramFiles%\Epic Games\UE_5.8`.
3. Supply the existing project's required NVIDIA UE 5.8 plugins under
   `SimCopterRemake/Plugins/NVIDIA`: DLSS, StreamlineCore, StreamlineDLSSG,
   StreamlineNGXCommon and StreamlineReflex. Local validation used NVIDIA bundle
   8.8.0. These build dependencies are not redistributed by this fork.
4. Supply your original game data in `Reference/SimCopterOriginalGame` as described
   in [the original data documentation](OriginalGameFileFormats.md). Runtime needs
   `bmp`, `cities`, `geo`, `sound`, `tweak` and `x`. The game build validates those
   directories and representative files. Neither this source fork nor Git LFS
   supplies the original game's commercial data.

## Editor and generated assets

Close the Unreal editor before compiling; from the repository root in PowerShell:

```powershell
cmd /c "RebuildUnrealCpp.bat < nul"
```

Use the wrapper for C++ builds. `Source/SimCopterRemake` is deliberately built
without unity compilation. See [build troubleshooting](BuildTroubleshooting.md).

Before packaging, generate locally derived original-game textures, intro movies
and loading art using the repository's `Tools/Unreal/BakeCityAtlas.py`,
`BakeIntroMovies.py` and `BakeLoadingScreen.py` workflows. The city atlas bake runs
inside Unreal; intro/loading conversion runs in Python with its documented tools.
See [packaging notes](memory/simcopter-packaged-build.md), [intro movies](IntroMovies.md)
and [loading screen](OriginalLoadingScreen.md).

The authored UFO, city materials, helicopter paint/glass, briefing WAVs, warning
sounds and Windows icon are retained in this fork. Their creator scripts are
under `Tools/Unreal`. After the original city atlas exists, run
`CreateDriveInMaterial.py` regenerates the included drive-in screen material and
fallback texture. MP4s stage as loose files in package-root `DriveInVideos`; their
SHA-256 values and dimensions are recorded in
`SimCopterRemake/Content/DriveInVideos/prepared-videos.json`. Run
`CreateHospitalPresentation.py` inside the editor to generate the locally baked
hospital materials and impact sound asset. Other reproducible material scripts
are `CreateUfoMaterials.py`, `CreateHelicopterPaint.py`, `CreateHelicopterGlass.py`
and `UpgradeCitySurfaces.py`.

## Tests and Shipping compilation

The project uses `SimCopter.*` Unreal automation tests. The critical police path is
`SimCopter.AirOperations.RecoveryCustodyControlsAndPersistence`, which creates
actual world actors, exercises firing and custody, and verifies that the Apache
cannot emit bullets or missiles in police taser mode.

```powershell
$Engine = 'C:\Program Files\Epic Games\UE_5.8'
$Project = (Resolve-Path 'SimCopterRemake/SimCopterRemake.uproject').Path
& "$Engine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" $Project `
  -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput `
  '-ExecCmds=Automation RunTests SimCopter;Quit' `
  '-TestExit=Automation Test Queue Empty'
cmd /c "RebuildUnrealCpp.bat Shipping < nul"
```

Do not run a headless editor while relying on the live editor MCP port. Consult
[AGENTS.md](../AGENTS.md) for the project's established validation workflow and
[ForkValidation.md](ForkValidation.md) for known test limitations.

The movie decoder/material and witness-photograph tests require rendering and are
excluded from NullRHI runs. Run those checks and enable the actual cheat/Gort Slate
previews separately:

```powershell
& "$Engine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" $Project `
  -unattended -nop4 -nosplash -RenderOffscreen -SimCheatDialogPreview -SimGortPreview `
  '-ExecCmds=Automation RunTests SimCopter.DriveIn.Playback+SimCopter.Witness.PhotoRendering+SimCopter.Cheats.OriginalDialogArt+SimCopter.Cheats.GortRender;Quit' `
  '-TestExit=Automation Test Queue Empty'
```

Packaging uses the existing RunUAT cook/stage/archive workflow. Original runtime
data stays loose under `<package>/SimCopter`, not inside the pak. Verify both maps,
the KINV/briefing/audio payloads and `Manifest_NonUFSFiles_Win64.txt` before
distributing a playable build. The source publication does not alter the existing
local installed game.
