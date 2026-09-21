param(
    [Parameter(Mandatory=$true)][string]$DependencyRoot
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$sourceRoot = Join-Path $projectRoot 'vendor\libbluray-1.5.0'
$buildRoot = Join-Path $projectRoot 'build\libbluray-x64'
$outputRoot = Join-Path $projectRoot 'out\libbluray-1.5.0-x64'
$venvRoot = Join-Path $PSScriptRoot '.venv'
$python = Join-Path $venvRoot 'Scripts\python.exe'
$meson = Join-Path $venvRoot 'Scripts\meson.exe'
$archive = Join-Path $projectRoot 'downloads\libbluray-1.5.0.tar.xz'
$expectedHash = 'f676408e91a5d321abf8b8d4dfdae36205c297dab5c54c3ec519639025f474a2'

function Invoke-Checked {
    param([string]$Executable, [string[]]$Arguments)
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Executable failed with exit code $LASTEXITCODE" }
}

foreach ($name in @('downloads', 'vendor', 'out', 'build')) {
    New-Item -ItemType Directory -Path (Join-Path $projectRoot $name) -Force | Out-Null
}
if (!(Test-Path -LiteralPath $python)) {
    Invoke-Checked -Executable 'python' -Arguments @('-m', 'venv', $venvRoot)
}
Invoke-Checked -Executable $python -Arguments @('-m', 'pip', 'install', '--disable-pip-version-check', '-r', (Join-Path $PSScriptRoot 'requirements-build.txt'))
if (!(Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest -Uri 'https://download.videolan.org/pub/videolan/libbluray/1.5.0/libbluray-1.5.0.tar.xz' -OutFile $archive
}
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash.ToLowerInvariant() -ne $expectedHash) {
    throw 'libbluray source archive SHA256 mismatch'
}
if (!(Test-Path -LiteralPath (Join-Path $sourceRoot 'meson.build'))) {
    Invoke-Checked -Executable $python -Arguments @((Join-Path $PSScriptRoot 'extract-source.py'), $archive, (Join-Path $projectRoot 'vendor'))
}
Invoke-Checked -Executable $python -Arguments @((Join-Path $PSScriptRoot 'libbluray-local-patch.py'), '--source', $sourceRoot)
foreach ($pc in @('freetype2.pc', 'libxml-2.0.pc', 'libudfread.pc')) {
    if (!(Test-Path -LiteralPath (Join-Path $DependencyRoot "lib\pkgconfig\$pc"))) {
        throw "Missing dependency: $DependencyRoot\lib\pkgconfig\$pc"
    }
}

$saved = @{}
foreach ($name in @('CC', 'CXX', 'PKG_CONFIG', 'PKG_CONFIG_PATH', 'PATH')) {
    $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
try {
    $env:CC = 'cl'
    $env:CXX = 'cl'
    $env:PKG_CONFIG = Join-Path $venvRoot 'Lib\site-packages\pkgconf\.bin\pkgconf.exe'
    $env:PKG_CONFIG_PATH = Join-Path $DependencyRoot 'lib\pkgconfig'
    $env:PATH = (Join-Path $venvRoot 'Scripts') + ';' + $env:PATH
    $setupArgs = @('setup', $buildRoot, $sourceRoot, '--vsenv', "--prefix=$outputRoot", '--buildtype=release', '--default-library=shared', '-Dbdj_jar=disabled', '-Dfontconfig=disabled', '-Dfreetype=enabled', '-Dlibxml2=enabled', '-Denable_tools=true', '-Denable_examples=true')
    if (Test-Path -LiteralPath (Join-Path $buildRoot 'meson-private\coredata.dat')) { $setupArgs += '--reconfigure' }
    Invoke-Checked -Executable $meson -Arguments $setupArgs
    Invoke-Checked -Executable $meson -Arguments @('compile', '-C', $buildRoot, '-j', '8')
    Invoke-Checked -Executable $meson -Arguments @('install', '-C', $buildRoot)

    $bin = Join-Path $outputRoot 'bin'
    foreach ($dll in @('udfread-3.dll', 'freetype.dll', 'libxml2.dll', 'brotlicommon.dll', 'brotlidec.dll', 'bz2.dll', 'libpng16.dll', 'z.dll', 'iconv-2.dll', 'charset-1.dll')) {
        Copy-Item -LiteralPath (Join-Path $DependencyRoot "bin\$dll") -Destination $bin
    }
    $licenseRoot = Join-Path $outputRoot 'licenses'
    Copy-Item -LiteralPath (Join-Path $projectRoot 'patches') -Destination $outputRoot -Recurse -Force
    New-Item -ItemType Directory -Path $licenseRoot -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $sourceRoot 'COPYING') -Destination (Join-Path $licenseRoot 'libbluray.txt')
    foreach ($package in @('libudfread', 'freetype', 'libxml2', 'brotli', 'bzip2', 'libpng', 'zlib', 'libiconv')) {
        Copy-Item -LiteralPath (Join-Path $DependencyRoot "share\$package\copyright") -Destination (Join-Path $licenseRoot "$package.txt")
    }

    $probeBuild = Join-Path $projectRoot 'build\probe-x64'
    $probeArgs = @('setup', $probeBuild, (Join-Path $projectRoot 'probe'), '--vsenv', '--buildtype=debugoptimized')
    if (Test-Path -LiteralPath (Join-Path $probeBuild 'meson-private\coredata.dat')) { $probeArgs += '--reconfigure' }
    Invoke-Checked -Executable $meson -Arguments $probeArgs
    Invoke-Checked -Executable $meson -Arguments @('compile', '-C', $probeBuild)
    Copy-Item -LiteralPath (Join-Path $probeBuild 'bluray-menu-probe.exe') -Destination $bin
    Copy-Item -LiteralPath (Join-Path $probeBuild 'bluray-mouse-probe.exe') -Destination $bin
    Copy-Item -LiteralPath (Join-Path $probeBuild 'bluray-bdj-probe.exe') -Destination $bin

    [ordered]@{
        libbluray = '1.5.0'
        local_revision = 'mouse-page-v1'
        local_patch_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot 'patches\libbluray-1.5.0-mouse-page.patch')).Hash.ToLowerInvariant()
        source_sha256 = $expectedHash
        architecture = 'x64'
        compiler = 'MSVC'
        bdj_jar_built = $false
        freetype = $true
        libxml2 = $true
        built_utc = [DateTime]::UtcNow.ToString('o')
        files = @(Get-ChildItem -LiteralPath $bin -File | ForEach-Object {
            [ordered]@{ name = $_.Name; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
        })
    } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $outputRoot 'build-manifest.json') -Encoding UTF8
    Write-Output "Built library and HDMV probe: $bin"
}
finally {
    foreach ($name in $saved.Keys) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') }
}
