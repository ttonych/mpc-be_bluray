param([string]$Disc = 'V:\', [string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
if (!$OutputDirectory) { $OutputDirectory = Join-Path $workspace ('diagnostics\runs\mouse-submenus-' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$bin = Join-Path $workspace 'out\libbluray-1.5.0-x64\bin'
& (Join-Path $bin 'bluray-mouse-probe.exe') $Disc $OutputDirectory (Join-Path $workspace 'probe\baby-boom-mouse-regression.txt') 1> (Join-Path $OutputDirectory 'events.jsonl') 2> (Join-Path $OutputDirectory 'stderr.log')
if ($LASTEXITCODE) { throw 'Mouse component probe failed.' }
& (Join-Path $PSScriptRoot '.venv\Scripts\python.exe') (Join-Path $PSScriptRoot 'check-mouse-probe.py') (Join-Path $OutputDirectory 'events.jsonl')
if ($LASTEXITCODE) { throw 'Mouse submenu regression failed; this fixture requires the BABY BOOM test disc.' }
