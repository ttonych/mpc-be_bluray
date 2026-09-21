param([string]$Fixture = '')
$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
foreach ($folder in @('build\tests', 'diagnostics\logs', 'diagnostics\fixtures')) {
    New-Item -ItemType Directory -Path (Join-Path $workspace $folder) -Force | Out-Null
}
$toolchain = if ($env:MPCBE_MSYS) { $env:MPCBE_MSYS } else { Join-Path $PSScriptRoot 'msys' }
$compiler = Join-Path $toolchain 'mingw\bin\g++.exe'
$target = Join-Path $workspace 'build\tests\bluray-menu-rle-test.exe'
& $compiler -std=c++17 -O2 -Wall -Wextra -static '-I' (Join-Path (Split-Path $workspace -Parent) 'include') '-o' $target (Join-Path $workspace 'probe\bluray-menu-rle-test.cpp')
if ($LASTEXITCODE) { throw 'Could not build the menu RLE regression test.' }
$arguments = @()
if ($Fixture) { $arguments += $Fixture }
$result = & $target @arguments
if ($LASTEXITCODE) { throw 'Menu RLE regression test failed.' }
$result | Set-Content -LiteralPath (Join-Path $workspace 'diagnostics\logs\bluray-menu-rle-test.log') -Encoding utf8
$result
