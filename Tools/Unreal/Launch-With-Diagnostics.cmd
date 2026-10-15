@echo off
setlocal
rem Install beside the normal launcher. One diagnostics launcher for everything: it records
rem GPU crash evidence (DRED/Aftermath breadcrumbs) and CPU/GPU/frame performance traces together.
rem Replaces the former Launch-With-GPU-Diagnostics.cmd and Launch-With-Performance-Diagnostics.cmd.
set "DIAGNOSTIC_EXE=%~dp0SimCopterRemake\Binaries\Win64\SimCopterRemake.exe"
if not exist "%DIAGNOSTIC_EXE%" (
    echo The Development diagnostic executable is missing.
    echo Download the Optional-Development archive matching your game version from GitHub.
    echo Extract its contents here, beside the normal SimCopterRemake.exe launcher.
    echo https://github.com/timmyty/sim-copter-remake/releases
    pause
    exit /b 1
)
set "SIMCOPTER_SAVED=%LOCALAPPDATA%\SimCopterRemake\Saved"
if not exist "%SIMCOPTER_SAVED%\Logs" mkdir "%SIMCOPTER_SAVED%\Logs"
if not exist "%SIMCOPTER_SAVED%\Profiling" mkdir "%SIMCOPTER_SAVED%\Profiling"
echo SimCopter Remake diagnostics: GPU crash evidence and performance traces.
echo Log:   %SIMCOPTER_SAVED%\Logs\Diagnostics.log
echo Trace: %SIMCOPTER_SAVED%\Profiling\
pushd "%~dp0"
"%DIAGNOSTIC_EXE%" -SaveToUserDir -gpucrashdebugging -nvaftermathall -slbinaries=production -trace=cpu,gpu,frame,bookmark,log -tracefile="%SIMCOPTER_SAVED%\Profiling\Diagnostics.utrace" -tracefiletimestamps -abslog="%SIMCOPTER_SAVED%\Logs\Diagnostics.log" %*
set "DIAGNOSTIC_EXIT=%ERRORLEVEL%"
popd
exit /b %DIAGNOSTIC_EXIT%
