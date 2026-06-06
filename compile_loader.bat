@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1
set "BASE=%~dp0"

:: Compilar
set BASE_=%BASE:~0,-1%
cl /EHsc /utf-8 /std:c++17 /I"%BASE_%\source\Cfg" /I"%BASE_%\keyauth" /I"%BASE_%\auth_bot" /I"%BASE_%" "%BASE_%\Loader.cpp" "%BASE_%\auth_bot\ka_bridge_api.cpp" /link user32.lib gdi32.lib wininet.lib winhttp.lib advapi32.lib shell32.lib /out:"%BASE_%\Satella_Loader.exe"
if %errorlevel% neq 0 echo ERRO na compilacao & pause & exit /b 1

:: Embed manifest (requireAdministrator)
"%windir%\System32\mt.exe" -manifest "%BASE_%\Satella_Loader.exe.manifest" -outputresource:"%BASE_%\Satella_Loader.exe";#1 2>nul
"C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\mt.exe" -manifest "%BASE_%\Satella_Loader.exe.manifest" -outputresource:"%BASE_%\Satella_Loader.exe";#1
echo Manifest embedado (admin)

:: Sign with certificate
echo.
echo Assinando com certificado...
"C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe" sign /fd SHA256 /f "%BASE_%\1.pfx" /p 123 /t http://timestamp.digicert.com "%BASE_%\Satella_Loader.exe"
if errorlevel 1 (
    echo.
    echo [!] Falha ao assinar. Verifique o certificado 1.pfx
) else (
    echo Assinado com certificado!
)

echo.
echo Loader compilado: Satella_Loader.exe
endlocal
pause
