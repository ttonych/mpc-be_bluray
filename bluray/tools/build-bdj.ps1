param([string]$JavaHome = $env:JAVA_HOME)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
$downloadRoot = Join-Path $workspace 'downloads'
New-Item -ItemType Directory -Path $downloadRoot -Force | Out-Null
if (!$JavaHome -or !(Test-Path -LiteralPath (Join-Path $JavaHome 'bin\javac.exe'))) {
    throw 'Pass -JavaHome with a JDK containing bin/javac.exe (tested with JDK 21). A JRE alone cannot compile the JARs.'
}
$javaRoot = (Resolve-Path -LiteralPath $JavaHome).Path
& python (Join-Path $PSScriptRoot 'libbluray-local-patch.py') --component bdj-toggle
if ($LASTEXITCODE) { throw 'BD-J source patch validation failed.' }
$antRoot = Join-Path $PSScriptRoot 'apache-ant-1.10.18'
$packages = @(
    @{ Name='apache-ant-1.10.18-bin.zip'; Url='https://downloads.apache.org/ant/binaries/apache-ant-1.10.18-bin.zip'; Algorithm='SHA512'; Hash='f86d7b263bc7c6903a91532943f1c8ecccc17dd999851a796617e79795908d7a666cbe999b9ecf06735cc7e830f50dca9f49b37bb5f6f02a0a3803cd22b558b0'; Target=$antRoot }
)
foreach ($package in $packages) {
    $archive = Join-Path $downloadRoot $package.Name
    if (!(Test-Path -LiteralPath $archive)) { Invoke-WebRequest -Uri $package.Url -OutFile $archive -TimeoutSec 60 }
    if ((Get-FileHash -LiteralPath $archive -Algorithm $package.Algorithm).Hash.ToLowerInvariant() -ne $package.Hash) { throw "Checksum mismatch: $archive" }
    if (!(Test-Path -LiteralPath $package.Target)) { Expand-Archive -LiteralPath $archive -DestinationPath $PSScriptRoot }
}
$savedJava = $env:JAVA_HOME
$savedPath = $env:PATH
try {
    $env:JAVA_HOME = $javaRoot
    $env:PATH = (Join-Path $javaRoot 'bin') + ';' + $savedPath
    $compilerVersion = & (Join-Path $javaRoot 'bin\javac.exe') -version 2>&1 | Out-String
    if ($LASTEXITCODE) { throw 'Could not run javac.' }
    $modernJdk = $compilerVersion -notmatch 'javac 1\.8\.'
    $sourceAsm = if ($modernJdk) { '1.8' } else { '1.5' }
    $sourceBdj = if ($modernJdk) { '1.8' } else { '1.4' }
    $compilerMajor = if ($compilerVersion -match 'javac (?:1\.)?(\d+)') { $Matches[1] } else { throw 'Unknown javac version.' }
    $build = Join-Path $workspace ('build\bdj-java' + $compilerMajor)
    $dist = Join-Path $workspace 'out\libbluray-1.5.0-x64\share\java'
    & (Join-Path $antRoot 'bin\ant.bat') -f (Join-Path $workspace 'vendor\libbluray-1.5.0\src\libbluray\bdj\build.xml') "-Dbuild=$build" "-Ddist=$dist" '-Dsrc_awt=:java-j2se:java-build-support' "-Djavac_path=$javaRoot\bin\javac.exe" '-Djavac_arg=-Xlint:-deprecation' '-Dversion=j2se-1.5.0' "-Djava_version_asm=$sourceAsm" "-Djava_version_bdj=$sourceBdj"
    if ($LASTEXITCODE) { throw 'BD-J Java build failed.' }
    [ordered]@{
        libbluray='1.5.0'; javac=$compilerVersion.Trim(); ant='1.10.18'
        local_revision='bdj-toggle-v1'
        local_patch_sha256=(Get-FileHash -LiteralPath (Join-Path $workspace 'patches\libbluray-1.5.0-bdj-toggle.patch') -Algorithm SHA256).Hash.ToLowerInvariant()
        built_utc=[DateTime]::UtcNow.ToString('o')
        packages=@($packages | ForEach-Object { @{name=$_.Name;url=$_.Url;algorithm=$_.Algorithm;hash=$_.Hash} })
        jars=@(Get-ChildItem -LiteralPath $dist -Filter '*.jar' | ForEach-Object { @{name=$_.Name;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()} })
    } | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $dist 'build-manifest.json') -Encoding utf8
    Write-Output "Built BD-J JARs: $dist"
} finally {
    $env:JAVA_HOME=$savedJava
    $env:PATH=$savedPath
}
