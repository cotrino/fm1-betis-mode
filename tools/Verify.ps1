[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$out = Join-Path $root 'docs'
function Hash($file) { (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() }
foreach ($line in Get-Content (Join-Path $out 'SHA256SUMS.txt')) {
    if ($line -cnotmatch '^([0-9a-f]{64})  ([A-Za-z0-9_.-]+)$') { throw 'Invalid checksum entry' }
    if ((Hash (Join-Path $out $Matches[2])) -cne $Matches[1]) { throw "Checksum mismatch: $line" }
}
$firmware = Get-Content (Join-Path $out 'firmware-report.json') -Raw | ConvertFrom-Json
$layout = Get-Content (Join-Path $out 'package-report.json') -Raw | ConvertFrom-Json
foreach ($field in @('passed','startup_entry_verified','registration_tables_verified','rtos_abi_verified',
    'audio_callbacks_in_sram','bluetooth_and_usb_midi_linked','usb_audio_playback_and_capture_linked',
    'application_callbacks_verified')) {
    if (!$firmware.$field) { throw "Firmware check failed: $field" }
}
if (!$layout.fits -or $layout.FLASH_NEED_SIZE -gt $layout.flash_bytes -or
    $layout.VM_REAL_SIZE -lt $layout.vm_minimum_bytes -or $layout.user_bytes -ne 536576) {
    throw 'Invalid flash/VM/user storage layout'
}
$html = [IO.File]::ReadAllText((Join-Path $out 'index.html'))
$webReport = Get-Content (Join-Path $out 'web-package-report.json') -Raw | ConvertFrom-Json
if ($webReport.web_sha256 -cne (Hash (Join-Path $out 'index.html'))) {
    throw 'Workbench report does not identify the distributed HTML'
}
foreach ($pair in @(@('FM1_PACKAGE','FM1_BETIS_MODE.fwsc'),@('FM1_SIMULATOR','simulator.wasm'))) {
    $match = [regex]::Match($html, ('(?s)const ' + $pair[0] + ' = (\{.*?\});'))
    if (!$match.Success) { throw "Missing embedded $($pair[0])" }
    $payload = $match.Groups[1].Value | ConvertFrom-Json
    $algorithm = [Security.Cryptography.SHA256]::Create()
    try {
        $digest = ([BitConverter]::ToString($algorithm.ComputeHash([Convert]::FromBase64String($payload.data)))).Replace('-','').ToLowerInvariant()
    } finally { $algorithm.Dispose() }
    if ($digest -cne $payload.sha256 -or $digest -cne (Hash (Join-Path $out $pair[1]))) {
        throw "Embedded payload differs from download: $($pair[0])"
    }
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [IO.Compression.ZipFile]::OpenRead((Join-Path $out 'source.zip'))
try {
    if (@($zip.Entries | Where-Object { $_.FullName.StartsWith('himnos/') }).Count) {
        throw 'Private user music must not enter the public corresponding-source archive'
    }
    foreach ($relative in @('Build.ps1','tools/Verify.ps1','web/betis-grada.html','web/betis-midi.js',
        'vendor/fm1_open/app/screen.c',
        'vendor/fm1_open/app/betis.c','vendor/fm1_open/app/betis.h',
        'vendor/fm1_open/web/engine/Simulator.c','vendor/fm1_open/sdk/manifest.json',
        'vendor/fm1_open/sdk/Build.ps1','LICENSE.md','THIRD_PARTY.md')) {
        $entry = $zip.GetEntry($relative)
        if (!$entry) { throw "Corresponding source missing: $relative" }
        $stream = $entry.Open()
        $algorithm = [Security.Cryptography.SHA256]::Create()
        try { $digest = ([BitConverter]::ToString($algorithm.ComputeHash($stream))).Replace('-','').ToLowerInvariant() }
        finally { $algorithm.Dispose(); $stream.Dispose() }
        if ($digest -cne (Hash (Join-Path $root $relative))) { throw "Stale corresponding source: $relative" }
    }
} finally { $zip.Dispose() }
Write-Host 'PASS: firmware ABI/SRAM checks, flash layout, all download hashes, embedded firmware/Wasm, and corresponding source.'
