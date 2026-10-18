[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$cache = Join-Path $root '.tools'
[IO.Directory]::CreateDirectory($cache) | Out-Null
function Download($url, $name, $sha256) {
    $file = Join-Path $cache $name
    if (!(Test-Path -LiteralPath $file) -or
        (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ine $sha256) {
        & curl.exe --fail --location --retry 2 --silent --show-error --output $file $url
        if ($LASTEXITCODE) { throw "Download failed: $url" }
    }
    if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ine $sha256) {
        throw "Download checksum mismatch: $name"
    }
    return $file
}
if (!$env:FM1_PI32_TOOLCHAIN) {
    $env:FM1_PI32_TOOLCHAIN = Join-Path $cache 'jieli-2.5.2\C$\JL\pi32'
    if (!(Test-Path -LiteralPath (Join-Path $env:FM1_PI32_TOOLCHAIN 'bin\clang.exe'))) {
        $extractor = Download 'https://constexpr.org/innoextract/files/innoextract-1.9-windows.zip' 'innoextract.zip' '6989342c9b026a00a72a38f23b62a8e6a22cc5de69805cf47d68ac2fec993065'
        Expand-Archive -LiteralPath $extractor -DestinationPath (Join-Path $cache 'innoextract') -Force
        $installer = Download 'https://jl-update.oss-cn-shenzhen.aliyuncs.com/2.5.2.exe' 'jieli-2.5.2.exe' '1ec78e3315a5987d4e82ecd002536c84240e5c832b1875beff7ce55450124ee5'
        & (Join-Path $cache 'innoextract\innoextract.exe') --silent --output-dir (Join-Path $cache 'jieli-2.5.2') $installer
        if ($LASTEXITCODE) { throw 'JieLi toolchain extraction failed' }
    }
}
$manifest = Get-Content (Join-Path $root 'vendor\fm1_open\sdk\manifest.json') -Raw | ConvertFrom-Json
foreach ($entry in $manifest.external_toolchain.files.PSObject.Properties) {
    $file = Join-Path $env:FM1_PI32_TOOLCHAIN $entry.Name
    if (!(Test-Path -LiteralPath $file) -or
        (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ine $entry.Value) {
        throw "Toolchain does not match official SDK: $($entry.Name)"
    }
}
if (!$env:FM1_WASI_SDK) {
    $env:FM1_WASI_SDK = Join-Path $cache 'wasi-sdk-27.0-x86_64-windows'
    if (!(Test-Path -LiteralPath (Join-Path $env:FM1_WASI_SDK 'bin\clang.exe'))) {
        $archive = Download 'https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-27/wasi-sdk-27.0-x86_64-windows.tar.gz' 'wasi-sdk.tar.gz' '4a576c13125c91996d8cc3b70b7ea0612c2044598d2795c9be100d15f874adf6'
        & tar.exe -xzf $archive -C $cache
        if ($LASTEXITCODE) { throw 'WASI SDK extraction failed' }
    }
}
Write-Host 'Toolchains ready; all PI32v2 SDK tool hashes verified.'
