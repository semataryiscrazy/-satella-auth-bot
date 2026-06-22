@echo off
setlocal enabledelayedexpansion

set "VSPATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
set "VC=%VSPATH%\VC\Tools\MSVC\14.44.35207"
set "WIN10KIT=C:\Program Files (x86)\Windows Kits\10"
set "WIN10VER=10.0.26100.0"

set "INCLUDE=%VC%\include;%WIN10KIT%\Include\%WIN10VER%\ucrt;%WIN10KIT%\Include\%WIN10VER%\um;%WIN10KIT%\Include\%WIN10VER%\shared;%WIN10KIT%\Include\%WIN10VER%\winrt"
set "LIB=%VC%\lib\x64;%WIN10KIT%\Lib\%WIN10VER%\ucrt\x64;%WIN10KIT%\Lib\%WIN10VER%\um\x64"

set "BASE_DIR=%~dp0"
set "OBJ_DIR=%BASE_DIR%launcher\ReleaseUD"
if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"

echo Compiling...
"%VC%\bin\HostX64\x64\cl.exe" /c ^
  "%BASE_DIR%launcher\LauncherUD.cpp" ^
  /I"%BASE_DIR%launcher" ^
  /Zi /nologo /W3 /WX- /diagnostics:column /sdl- /O2 /Oi /GL ^
  /D WIN32 /D NDEBUG /D _WINDOWS /D _UNICODE /D UNICODE ^
  /Gm- /EHsc /MT /GS /Gy /fp:precise /Zc:wchar_t /Zc:forScope /Zc:inline ^
  /Fo"%OBJ_DIR%\\" /Fd"%OBJ_DIR%\vc143.pdb" /external:W3 /Gd /TP /FC

if %errorlevel% neq 0 (
  echo Compile failed!
  pause
  exit /b 1
)

set "PATH=%WIN10KIT%\bin\%WIN10VER%\x64;%PATH%"
set "LIB=%LIB%;%WIN10KIT%\Lib\%WIN10VER%\ucrt\x64;%WIN10KIT%\Lib\%WIN10VER%\um\x64"

set "OUTEXE=%BASE_DIR%x64\Release\MediaCreationTool.exe"

echo Linking MediaCreationTool.exe...
"%VC%\bin\HostX64\x64\link.exe" ^
  /OUT:"%OUTEXE%" ^
  /INCREMENTAL:NO /NOLOGO ^
  "%OBJ_DIR%\LauncherUD.obj" ^
  kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib ^
  advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib ^
  odbc32.lib odbccp32.lib ^
  /MANIFEST /MANIFESTUAC:"level='asInvoker' uiAccess='false'" /manifest:embed ^
  /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /LTCG:incremental ^
  /DYNAMICBASE /NXCOMPAT /MACHINE:X64

if %errorlevel% neq 0 (
  echo Link failed!
  pause
  exit /b 1
)

echo Signing with certificate...
"%WIN10KIT%\bin\%WIN10VER%\x64\signtool.exe" sign /fd SHA256 /f "%BASE_DIR%1.pfx" /p 123 /t http://timestamp.digicert.com "%OUTEXE%"

echo Copying to loader folder...
copy /Y "%OUTEXE%" "%BASE_DIR%..\loader\MediaCreationTool.exe" >nul 2>&1
if exist "%BASE_DIR%..\loader\MediaCreationTool.exe" (
  echo Copied to ..\loader\MediaCreationTool.exe
)

echo Done.
pause
