param([string]$VcpkgRoot)
$ErrorActionPreference = 'Stop'
$component = Split-Path $PSScriptRoot -Parent
$downloads = Join-Path $component 'downloads'
New-Item -ItemType Directory -Path $downloads -Force | Out-Null
$toolchain = Join-Path $PSScriptRoot 'msys'
$archive = Join-Path $downloads 'MSYS_MinGW-w64_GCC_1521_x86-x64.7z'
$expected = 'ac66f98f21ebe8271871bc9f6d4f44f8e1efb9c35e26ded519469746b10b713c'
if (!(Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/Aleksoid1978/MSYS/3d89b54078e6410b625a618367c7f640da6b9802/MSYS_MinGW-w64_GCC_1521_x86-x64.7z' -OutFile $archive
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) { throw 'MSYS checksum mismatch.' }
if (!(Test-Path -LiteralPath (Join-Path $toolchain 'mingw\bin\gcc.exe'))) {
    & 7z x $archive "-o$toolchain" -y | Out-Null
    if ($LASTEXITCODE) { throw 'MSYS extraction failed.' }
}
$manifest = Get-Content -LiteralPath (Join-Path $component 'vcpkg.json') -Raw | ConvertFrom-Json
if (!$VcpkgRoot) { $VcpkgRoot = Join-Path $component 'build\vcpkg' }
if (!(Test-Path -LiteralPath (Join-Path $VcpkgRoot '.git'))) {
    git clone --filter=blob:none --no-checkout https://github.com/microsoft/vcpkg.git $VcpkgRoot
    if ($LASTEXITCODE) { throw 'vcpkg clone failed.' }
    git -C $VcpkgRoot checkout --detach $manifest.'builtin-baseline'
    if ($LASTEXITCODE) { throw 'vcpkg checkout failed.' }
}
$actualCommit = & git -C $VcpkgRoot rev-parse HEAD
if ($LASTEXITCODE -or $actualCommit -ne $manifest.'builtin-baseline') { throw 'vcpkg must match the pinned baseline; existing checkouts are not modified.' }
if (!(Test-Path -LiteralPath (Join-Path $VcpkgRoot 'vcpkg.exe'))) {
    & (Join-Path $VcpkgRoot 'bootstrap-vcpkg.bat') -disableMetrics
    if ($LASTEXITCODE) { throw 'vcpkg bootstrap failed.' }
}
$installRoot = Join-Path $component 'build\vcpkg_installed'
& (Join-Path $VcpkgRoot 'vcpkg.exe') install "--x-manifest-root=$component" "--x-install-root=$installRoot" --triplet x64-windows --disable-metrics
if ($LASTEXITCODE) { throw 'vcpkg dependency build failed.' }
Write-Output "Dependencies: $installRoot\x64-windows"
