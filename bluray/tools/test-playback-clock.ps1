$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
foreach ($folder in @('build\tests', 'diagnostics\logs', 'diagnostics\fixtures')) {
    New-Item -ItemType Directory -Path (Join-Path $workspace $folder) -Force | Out-Null
}
$toolchain = if ($env:MPCBE_MSYS) { $env:MPCBE_MSYS } else { Join-Path $PSScriptRoot 'msys' }
$compiler = Join-Path $toolchain 'mingw\bin\g++.exe'
$source = Join-Path $workspace 'probe\bluray-playback-clock-test.cpp'
$target = Join-Path $workspace 'build\tests\bluray-playback-clock-test.exe'
& $compiler -std=c++17 -O2 -Wall -Wextra -o $target $source
if ($LASTEXITCODE) { throw 'Could not build the playback clock regression test.' }
$result = & $target
if ($LASTEXITCODE) { throw 'Playback clock regression test failed.' }
$result | Set-Content (Join-Path $workspace 'diagnostics\logs\bluray-playback-clock-test.log') -Encoding utf8
$result
