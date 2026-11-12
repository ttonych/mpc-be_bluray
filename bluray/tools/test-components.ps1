param([string]$JavaHome = $env:JAVA_HOME)
$ErrorActionPreference = 'Stop'
foreach ($name in @('bdj-argb','disc-storage','media-monitor','menu-background','menu-audio','file-retry','java-runtime','menu-color','menu-coordinates','menu-rle','playback-clock','portable-profile','release-version')) {
    & (Join-Path $PSScriptRoot "test-$name.ps1")
}
foreach ($component in @('mouse-page','native','bdj-toggle','bdj-container-repaint')) {
    python (Join-Path $PSScriptRoot 'test-libbluray-local-patch.py') --component $component
    if ($LASTEXITCODE) { throw "Patch regression failed: $component" }
}
foreach ($name in @('process-query-hook', 'playmark-seek')) {
    python (Join-Path $PSScriptRoot "test-$name.py")
    if ($LASTEXITCODE) { throw "Regression failed: $name" }
}
& (Join-Path $PSScriptRoot 'test-bdj-toggle.ps1') -JavaHome $JavaHome
$bdjJar = Join-Path $PSScriptRoot '../out/libbluray-1.5.0-x64/share/java/libbluray-j2se-1.5.0.jar'
& (Join-Path $JavaHome 'bin/java.exe') '-Djava.awt.headless=true' -cp $bdjJar (Join-Path $PSScriptRoot '../probe/ContainerRepaintTest.java')
if ($LASTEXITCODE) { throw 'HAVi container repaint regression failed.' }
python (Join-Path $PSScriptRoot 'test-public-check.py')
if ($LASTEXITCODE) { throw 'Publication checker regression failed.' }
