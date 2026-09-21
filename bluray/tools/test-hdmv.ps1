param(
    [string]$Disc = 'V:\',
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (!$OutputDirectory) {
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $OutputDirectory = Join-Path $projectRoot "diagnostics\runs\hdmv-$stamp"
}
$bin = Join-Path $projectRoot 'out\libbluray-1.5.0-x64\bin'
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
& (Join-Path $bin 'bd_info.exe') $Disc 1> (Join-Path $OutputDirectory 'disc-info.txt') 2> (Join-Path $OutputDirectory 'disc-info-stderr.log')
if ($LASTEXITCODE -ne 0) { throw "Disc information probe failed: $LASTEXITCODE" }
& (Join-Path $bin 'bluray-menu-probe.exe') $Disc $OutputDirectory 1> (Join-Path $OutputDirectory 'events.jsonl') 2> (Join-Path $OutputDirectory 'stderr.log')
$probeExit = $LASTEXITCODE
$events = @(Get-Content -LiteralPath (Join-Path $OutputDirectory 'events.jsonl') | ForEach-Object { $_ | ConvertFrom-Json })
$summary = $events | Where-Object { $_.type -eq 'summary' } | Select-Object -Last 1
$summary | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputDirectory 'summary.json') -Encoding UTF8
$summary | Format-List
Write-Output "Evidence: $OutputDirectory"
if ($probeExit -ne 0 -or !$summary -or !$summary.passed) { throw "HDMV probe failed: $probeExit" }
