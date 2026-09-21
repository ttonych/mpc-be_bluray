param(
    [Parameter(Mandatory=$true)][string]$JavaHome,
    [string]$Jar
)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
if (!$Jar) { $Jar = Join-Path $workspace 'out\libbluray-1.5.0-x64\share\java\libbluray-j2se-1.5.0.jar' }
$jarPath = (Resolve-Path -LiteralPath $Jar).Path
$testRoot = Join-Path $workspace 'probe\bdj-toggle-test'
$classes = Join-Path $workspace 'build\bdj-toggle-test'
New-Item -ItemType Directory -Path $classes -Force | Out-Null
$sources = @(Get-ChildItem -LiteralPath $testRoot -Recurse -Filter '*.java' | ForEach-Object FullName)
& (Join-Path $JavaHome 'bin\javac.exe') -encoding UTF-8 -cp $jarPath -d $classes @sources
if ($LASTEXITCODE) { throw 'Could not compile HAVi regression test.' }
& (Join-Path $JavaHome 'bin\java.exe') '-Djava.awt.headless=true' -cp "$classes;$jarPath" ToggleTest
if ($LASTEXITCODE) { throw 'HAVi toggle regression test failed.' }
