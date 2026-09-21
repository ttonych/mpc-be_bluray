param([string]$KingdomDisc = '')
$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
foreach ($folder in @('build\tests', 'diagnostics\logs', 'diagnostics\fixtures')) {
    New-Item -ItemType Directory -Path (Join-Path $workspace $folder) -Force | Out-Null
}
$toolchain = if ($env:MPCBE_MSYS) { $env:MPCBE_MSYS } else { Join-Path $PSScriptRoot 'msys' }
$compiler = Join-Path $toolchain 'mingw\bin\x86_64-w64-mingw32-g++.exe'
$target = Join-Path $workspace 'build\tests\bluray-menu-background-test.exe'
& $compiler -m64 -std=c++17 -O2 -Wall -Wextra -static '-I' (Join-Path (Split-Path $workspace -Parent) 'include') '-o' $target (Join-Path $workspace 'probe\bluray-menu-background-test.cpp')
if ($LASTEXITCODE) { throw 'Could not build the menu background regression test.' }
$arguments = @()
if ($KingdomDisc) { $arguments = @((Join-Path $workspace 'out\libbluray-1.5.0-x64\bin\bluray-4.dll'), $KingdomDisc) }
$result = & $target @arguments
if ($LASTEXITCODE) { throw 'Menu background regression test failed.' }
$result | Set-Content -LiteralPath (Join-Path $workspace 'diagnostics\logs\bluray-menu-background-test.log') -Encoding utf8
$result
