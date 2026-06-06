@echo off
cd /d "C:\Users\s\Downloads\satella crazy"
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "launcher\Launcher.vcxproj" /p:Configuration=Release /p:Platform=x64
if %errorlevel% neq 0 (
    echo Build failed
    pause
    exit /b 1
)
set EXE="x64\Release\MediaCreationTool.exe"
set MANIFEST="launcher\MediaCreationTool.exe.manifest"
set CERT="1.pfx"
set PASSWORD=123

echo Embedding manifest...
"C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\mt.exe" -manifest %MANIFEST% -outputresource:%EXE%;#1

echo Signing with certificate...
"C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe" sign /fd SHA256 /f %CERT% /p %PASSWORD% /t http://timestamp.digicert.com %EXE%

echo Done: %EXE% is signed
pause
