param([string]$JavaHome)
$ErrorActionPreference = 'Stop'
$component = Split-Path $PSScriptRoot -Parent
$toolchain = if ($env:MPCBE_MSYS) { $env:MPCBE_MSYS } else { Join-Path $PSScriptRoot 'msys' }
$target = Join-Path $component 'build/tests/java-runtime-test.exe'
New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
& (Join-Path $toolchain 'mingw/bin/g++.exe') -std=c++17 -O2 -Wall -Wextra -static -municode -o $target (Join-Path $component 'probe/java-runtime-test.cpp') -ladvapi32
if ($LASTEXITCODE) { throw 'Could not build Java runtime tests.' }
$fixture = Join-Path $component ('diagnostics/fixtures/java-runtime-' + [guid]::NewGuid().ToString('N'))
if ($JavaHome) { & $target $fixture $JavaHome } else { & $target $fixture }
if ($LASTEXITCODE) { throw 'Java runtime tests failed.' }
