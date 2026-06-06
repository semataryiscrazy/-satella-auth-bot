$ErrorActionPreference = 'Stop'
$dll = "C:\Users\s\Downloads\void attt byy tecxas\void attt byy tecxas\void attt by tecxas\void attt by tecxas\x64\Release\Satella.dll"
$out = "C:\Users\s\Downloads\void attt byy tecxas\void attt byy tecxas\void attt by tecxas\void attt by tecxas\embedded_dll.h"
$key = @(0xE7,0x3B,0x8D,0x52,0xF1,0xA4,0x6C,0x90,0x2E,0x5B,0xC9,0x14,0xD8,0x76,0x0A,0xF3,0x49,0xBE,0x65,0x81,0x37,0xCC,0x5F,0x9E,0x21,0x0D,0xB4,0x78,0xC2,0xE1,0x4D,0x93)

Write-Host "Reading DLL..."
$data = [System.IO.File]::ReadAllBytes($dll)
$len = $data.Length
Write-Host ("Read {0} bytes" -f $len)

Write-Host "XORing..."
for ($i = 0; $i -lt $len; $i++) {
    $data[$i] = $data[$i] -bxor $key[$i % $key.Length]
}

Write-Host "Writing header..."
$sw = New-Object System.IO.StreamWriter($out, $false, [System.Text.Encoding]::ASCII)
$sw.WriteLine('#pragma once')
$sw.WriteLine('#include <stddef.h>')
$sw.WriteLine('#include <windows.h>')
$sw.WriteLine("static const BYTE g_embeddedDll[{0}] = {{", $len)
for ($i = 0; $i -lt $len; $i += 32) {
    $sb = New-Object System.Text.StringBuilder
    $end = $i + 32
    if ($end -gt $len) { $end = $len }
    for ($j = $i; $j -lt $end; $j++) {
        $sb.AppendFormat("0x{0:X2},", $data[$j]) | Out-Null
    }
    $sw.Write('  ')
    $sw.WriteLine($sb.ToString())
}
$sw.WriteLine('};')
$sw.WriteLine("static const size_t g_embeddedDllSz = {0};", $len)
$sw.Close()

Write-Host ("OK - {0} bytes -> embedded_dll.h" -f $len)
