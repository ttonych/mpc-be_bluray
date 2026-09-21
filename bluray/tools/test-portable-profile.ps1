param([switch]$ReadExistingProfile)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
foreach ($folder in @('build\tests', 'diagnostics\logs', 'diagnostics\fixtures')) {
    New-Item -ItemType Directory -Path (Join-Path $workspace $folder) -Force | Out-Null
}
$toolchain = if ($env:MPCBE_MSYS) { $env:MPCBE_MSYS } else { Join-Path $PSScriptRoot 'msys' }
$compiler = Join-Path $toolchain 'mingw\bin\g++.exe'
$target = Join-Path $workspace 'build\tests\portable-profile-test.exe'
& $compiler -std=c++17 -O2 -Wall -Wextra -static -municode -o $target (Join-Path $workspace 'probe\portable-profile-test.cpp') -lcrypt32 -ladvapi32
if ($LASTEXITCODE) { throw 'Could not build portable profile tests.' }
$fixture = Join-Path $workspace ('diagnostics\fixtures\portable-profile-fixture-' + [guid]::NewGuid().ToString('N'))
if ($ReadExistingProfile) { $result = & $target $fixture read-existing }
else { $result = & $target $fixture }
if ($LASTEXITCODE) { $result; throw "Profile tests failed; inspect $fixture" }
$result | Set-Content -LiteralPath (Join-Path $workspace 'diagnostics\logs\portable-profile-test.log') -Encoding utf8
$result
