[CmdletBinding()]
param(
    [string]$SourceUrl = 'https://cotrino.github.io/fm1-betis-mode/source.zip'
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$upstream = Join-Path $root 'vendor\fm1_open'
$out = Join-Path $root 'docs'
$utf8 = New-Object Text.UTF8Encoding $false
& (Join-Path $root 'tools\Get-Toolchains.ps1')
& (Join-Path $upstream 'sdk\Build.ps1') -Package -SourceUrl $SourceUrl
$report = Get-Content (Join-Path $upstream 'build\dx7\firmware-report.json') -Raw | ConvertFrom-Json
$layout = Get-Content (Join-Path $upstream 'build\dx7\package-report.json') -Raw | ConvertFrom-Json
if (!$report.passed -or !$layout.fits) { throw 'Official firmware validation failed' }
[IO.Directory]::CreateDirectory($out) | Out-Null
Copy-Item (Join-Path $upstream 'build\dx7\generated\FM1_OFFICIAL_OPEN.fwsc') (Join-Path $out 'FM1_BETIS_MODE.fwsc') -Force
Copy-Item (Join-Path $upstream 'build\web\simulator.wasm') $out -Force
foreach ($name in @('firmware-report.json','package-report.json','web-package-report.json','build-identity.json')) {
    Copy-Item (Join-Path $upstream "build\dx7\$name") $out -Force
}
foreach ($name in @('LICENSE.md','THIRD_PARTY.md')) { Copy-Item (Join-Path $root $name) $out -Force }
$html = [IO.File]::ReadAllText((Join-Path $upstream 'build\official-web\FM1-Official-Open-Source.html'))
# The SDK remains byte-for-byte intact. Customize only the generated derivative.
$playable = '[0,2,3,4,5,10,15,17].includes(state[1])'
if (!$html.Contains($playable)) { throw 'Upstream browser playable-page contract changed' }
$html = $html.Replace($playable, '[0,2,3,4,5,10,15,17,19].includes(state[1])')
$shortcut = '<div class="edit-shortcuts"><button data-sim data-sim-page="15"'
if (!$html.Contains($shortcut)) { throw 'Upstream browser toolbar contract changed' }
$html = $html.Replace($shortcut, '<div class="edit-shortcuts"><button data-sim data-sim-page="19" disabled>Betis Mode</button><button data-sim data-sim-page="15"')
$banner = @'
<aside style="margin:16px;padding:16px;border:2px solid #22c55e;background:#102619;color:white">
<strong>FM1 BETIS MODE - unofficial community firmware</strong>
<p>Real FM1 Open engine and 240x240 TFT emulator. HOME+FX: Betis dashboard.
SCL on dashboard: scale on/off. Knobs: FM feedback, reverb send, delay send, BPM.</p>
<p><a href="FM1_BETIS_MODE.fwsc">Download firmware</a> |
<a href="source.zip">Matching source + SDK</a> |
<a href="firmware-report.json">Build verification</a> |
<a href="LICENSE.md">License</a> | <a href="THIRD_PARTY.md">Notices</a></p>
<p>Build and package checks passed. NOT tested on physical hardware.
Back up projects and samples before updating. No automatic device flashing.</p>
</aside>
'@
if (!$html.Contains('<main>')) { throw 'Upstream browser main contract changed' }
$html = $html.Replace('<main>', "<main>`n$banner")
$html = [regex]::Replace($html, '<title>[^<]*</title>', '<title>FM1 Betis Mode Workbench</title>')
[IO.File]::WriteAllText((Join-Path $out 'index.html'), $html, $utf8)
[IO.File]::WriteAllText((Join-Path $out '.nojekyll'), '', $utf8)
$acceptance = [IO.File]::ReadAllText((Join-Path $root 'tests\verify-emulator.html')).Replace('../docs/simulator.wasm', 'simulator.wasm')
[IO.File]::WriteAllText((Join-Path $out 'verify-emulator.html'), $acceptance, $utf8)
$webReportFile = Join-Path $out 'web-package-report.json'
$webReport = Get-Content $webReportFile -Raw | ConvertFrom-Json
$webReport | Add-Member -NotePropertyName upstream_web_sha256 -NotePropertyValue $webReport.web_sha256
$webReport.web_sha256 = (Get-FileHash (Join-Path $out 'index.html') -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText($webReportFile, ($webReport | ConvertTo-Json), $utf8)

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$source = Join-Path $out 'source.zip'
if (Test-Path -LiteralPath $source) { Remove-Item -LiteralPath $source }
$zip = [IO.Compression.ZipFile]::Open($source, [IO.Compression.ZipArchiveMode]::Create)
try {
    $roots = @('src','web','tests','tools','.github','vendor\fm1_open\app','vendor\fm1_open\sdk',
        'vendor\fm1_open\web','vendor\fm1_open\tools','vendor\fm1_open\third_party')
    $files = @()
    foreach ($dir in $roots) { $files += Get-ChildItem -LiteralPath (Join-Path $root $dir) -Recurse -File }
    $files += Get-ChildItem -LiteralPath $root -File
    $files += Get-ChildItem -LiteralPath $upstream -File
    foreach ($file in $files | Sort-Object FullName -Unique) {
        $relative = $file.FullName.Substring($root.Length + 1).Replace('\','/')
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $file.FullName, $relative,
            [IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
} finally { $zip.Dispose() }
$sums = Get-ChildItem -LiteralPath $out -File | Where-Object { $_.Name -ne 'SHA256SUMS.txt' } |
    Sort-Object Name | ForEach-Object { (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() + '  ' + $_.Name }
[IO.File]::WriteAllLines((Join-Path $out 'SHA256SUMS.txt'), $sums, $utf8)
Write-Host "Verified firmware, workbench, corresponding source, licenses and checksums: $out"
