$ErrorActionPreference = 'Stop'
$component = Split-Path $PSScriptRoot -Parent
$toolchain = if ($env:MPCBE_MSYS) { $env:MPCBE_MSYS } else { Join-Path $PSScriptRoot 'msys' }
$target = Join-Path $component 'build/tests/release-version-test.exe'
New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
& (Join-Path $toolchain 'mingw/bin/g++.exe') -std=c++17 -O2 -Wall -Wextra -static -o $target (Join-Path $component 'probe/release-version-test.cpp')
if ($LASTEXITCODE) { throw 'Could not build release version tests.' }
& $target
if ($LASTEXITCODE) { throw 'Release version tests failed.' }
