$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
foreach ($folder in @('build\tests', 'diagnostics\logs', 'diagnostics\fixtures')) {
    New-Item -ItemType Directory -Path (Join-Path $workspace $folder) -Force | Out-Null
}
$toolchain = if ($env:MPCBE_MSYS) { $env:MPCBE_MSYS } else { Join-Path $PSScriptRoot 'msys' }
$compiler = Join-Path $toolchain 'mingw\bin\g++.exe'
$target = Join-Path $workspace 'build\tests\bluray-disc-storage-test.exe'
& $compiler -std=c++17 -O2 -Wall -Wextra -static -municode -o $target (Join-Path $workspace 'probe\bluray-disc-storage-test.cpp') -lbcrypt -lole32
if ($LASTEXITCODE) { throw 'Could not build the disc storage test.' }
$fixture = Join-Path $workspace ('diagnostics\fixtures\disc-storage-fixture-' + [guid]::NewGuid().ToString('N'))
$result = & $target $fixture
if ($LASTEXITCODE) { $result; throw "Storage test failed; inspect $fixture" }
$result | Set-Content -LiteralPath (Join-Path $workspace 'diagnostics\logs\bluray-disc-storage-test.log') -Encoding utf8
$result
