@echo off
setlocal
rem Install beside the normal launcher. Record frame timings and GPU crash evidence together.
rem Keep the same DRED/Aftermath flags as Launch-With-GPU-Diagnostics.cmd.
set "PERFORMANCE_EXE=%~dp0SimCopterRemake\Binaries\Win64\SimCopterRemake.exe"
if not exist "%PERFORMANCE_EXE%" (
    echo The Development diagnostic executable is missing.
    echo Build RebuildUnrealCpp.bat Development and install it beside the Shipping executable.
    pause
    exit /b 1
)
pushd "%~dp0"
"%PERFORMANCE_EXE%" -SaveToUserDir -gpucrashdebugging -nvaftermathall -slbinaries=production -trace=cpu,gpu,frame,bookmark,log -tracefile="%LOCALAPPDATA%\SimCopterRemake\Saved\Profiling\Witness-Performance.utrace" -tracefiletimestamps -abslog="%LOCALAPPDATA%\SimCopterRemake\Saved\Logs\Performance-Diagnostics.log" %*
set "PERFORMANCE_EXIT=%ERRORLEVEL%"
popd
exit /b %PERFORMANCE_EXIT%
