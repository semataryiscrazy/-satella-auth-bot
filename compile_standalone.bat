@echo off
REM ??? Satella Standalone Overlay Build Script ???
REM Requires Visual Studio 2022 (v143 toolset)
REM Run from the repository root directory

echo Building Satella Standalone Overlay...
echo.

REM Check for MSBuild
set MSBUILD="C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
if not exist %MSBUILD% (
    set MSBUILD="C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
)
if not exist %MSBUILD% (
    echo MSBuild not found. Please ensure Visual Studio 2022 is installed.
    pause
    exit /b 1
)

REM Build the standalone project
echo Building x64 Release...
%MSBUILD% "standalone\SatellaOverlay.vcxproj" /p:Configuration=Release /p:Platform=x64 /t:Build

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build successful! Output: x64\Release\SatellaOverlay.exe
) else (
    echo.
    echo Build failed! Check errors above.
)

pause
