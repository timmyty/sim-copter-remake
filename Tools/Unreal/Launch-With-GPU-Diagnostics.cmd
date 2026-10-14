@echo off
setlocal
rem Install this launcher in the Windows package root beside the normal launcher.
rem Development includes GPU breadcrumbs and logs that Shipping compiles out.
set "DIAGNOSTIC_EXE=%~dp0SimCopterRemake\Binaries\Win64\SimCopterRemake.exe"
if not exist "%DIAGNOSTIC_EXE%" (
    echo The Development diagnostic executable is missing.
    echo Download the Optional-Development archive matching your game version from GitHub.
    echo Extract its contents here, beside the normal SimCopterRemake.exe launcher.
    echo https://github.com/timmyty/sim-copter-remake/releases
    pause
    exit /b 1
)
pushd "%~dp0"
"%DIAGNOSTIC_EXE%" -SaveToUserDir -gpucrashdebugging -nvaftermathall -slbinaries=production -abslog="%LOCALAPPDATA%\SimCopterRemake\Saved\Logs\GPU-Diagnostics.log"
set "DIAGNOSTIC_EXIT=%ERRORLEVEL%"
popd
exit /b %DIAGNOSTIC_EXIT%
