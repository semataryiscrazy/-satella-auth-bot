@echo off
REM Build SatellaLoader standalone EXE
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1
set "BASE=%~dp0"
echo Building SatellaLoader x64 Release...
msbuild "%BASE%standalone\SatellaLoader.vcxproj" /p:Configuration=Release /p:Platform=x64 /t:Build
if %ERRORLEVEL% EQU 0 (
    echo Build successful! Output: %BASE%x64\Release\SatellaLoader.exe
) else (
    echo Build failed!
)
pause
endlocal
