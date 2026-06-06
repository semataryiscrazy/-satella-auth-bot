$ErrorActionPreference = 'Stop'
$base = $args[0]
$dll = "$base\x64\Release\Satella.dll"
$xorFile = "$base\Satella.xor"
$rcFile = "$base\embedded.rc"
$key = @(0xE7,0x3B,0x8D,0x52,0xF1,0xA4,0x6C,0x90,0x2E,0x5B,0xC9,0x14,0xD8,0x76,0x0A,0xF3,0x49,0xBE,0x65,0x81,0x37,0xCC,0x5F,0x9E,0x21,0x0D,0xB4,0x78,0xC2,0xE1,0x4D,0x93)

Write-Host "Reading DLL..."
$data = [System.IO.File]::ReadAllBytes($dll)
Write-Host ("XORing {0} bytes..." -f $data.Length)
for ($i = 0; $i -lt $data.Length; $i++) { $data[$i] = $data[$i] -bxor $key[$i % $key.Length] }
[System.IO.File]::WriteAllBytes($xorFile, $data)
Write-Host ("Wrote {0}" -f $xorFile)

$rcContent = @"
#include <windows.h>
IDR_SATELLA_DLL RCDATA "${xorFile}"
"@
[System.IO.File]::WriteAllText($rcFile, $rcContent, [System.Text.Encoding]::ASCII)
Write-Host ("Wrote {0}" -f $rcFile)
