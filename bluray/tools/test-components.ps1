param([string]$JavaHome = $env:JAVA_HOME)
$ErrorActionPreference = 'Stop'
foreach ($name in @('bdj-argb','disc-storage','media-monitor','menu-background','menu-color','menu-coordinates','menu-rle','playback-clock','portable-profile','release-version')) {
    & (Join-Path $PSScriptRoot "test-$name.ps1")
}
foreach ($component in @('mouse-page','bdj-toggle')) {
    python (Join-Path $PSScriptRoot 'test-libbluray-local-patch.py') --component $component
    if ($LASTEXITCODE) { throw "Patch regression failed: $component" }
}
& (Join-Path $PSScriptRoot 'test-bdj-toggle.ps1') -JavaHome $JavaHome
python (Join-Path $PSScriptRoot 'test-public-check.py')
if ($LASTEXITCODE) { throw 'Publication checker regression failed.' }
