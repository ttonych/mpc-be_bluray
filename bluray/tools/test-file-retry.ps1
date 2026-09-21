$ErrorActionPreference = 'Stop'
$component = Split-Path $PSScriptRoot -Parent
$source = Split-Path $component -Parent
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.ATLMFC -property installationPath
if (!$vs) { throw 'Visual Studio 2022 with ATL/MFC is required.' }
$output = Join-Path $component 'build\tests\file-retry'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$batch = Join-Path $output 'build.cmd'
@"
@echo off
call "$vs\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /MD /D_AFXDLL /DUNICODE /D_UNICODE /I"$source\include" /I"$source\src" "$component\probe\bluray-file-retry-test.cpp" /Fe:file-retry-test.exe /link shlwapi.lib
"@ | Set-Content -LiteralPath $batch -Encoding ascii
Push-Location $output
try {
    & $env:ComSpec /d /c $batch
    if ($LASTEXITCODE) { throw 'Could not build the file retry regression test.' }
    & (Join-Path $output 'file-retry-test.exe') (Join-Path $output ('fixture-' + [guid]::NewGuid().ToString('N') + '.bin'))
    if ($LASTEXITCODE) { throw 'File retry regression failed.' }
} finally { Pop-Location }
