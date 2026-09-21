param(
    [string]$SdkVersion = '10.0.19041.0',
    [string]$ToolchainRoot = $env:MPCBE_MSYS,
    [string]$RuntimeDirectory,
    [switch]$StandardProfile
)
$ErrorActionPreference = 'Stop'
$component = Split-Path $PSScriptRoot -Parent
$source = Split-Path $component -Parent
if (!$ToolchainRoot) { $ToolchainRoot = Join-Path $PSScriptRoot 'msys' }
if (!$RuntimeDirectory) { $RuntimeDirectory = Join-Path $component 'out\libbluray-1.5.0-x64' }
$runtime = (Resolve-Path -LiteralPath $RuntimeDirectory).Path
$gccRoot = Join-Path $ToolchainRoot 'mingw'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.ATLMFC -property installationPath
if (!$vs) { throw 'Visual Studio 2022 with ATL/MFC is required.' }
$msbuild = Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
$gccVersion = & (Join-Path $gccRoot 'bin\gcc.exe') -dumpversion
if ($LASTEXITCODE) { throw 'The MPC-BE GCC/MSYS toolchain is required.' }
foreach ($path in @('bin\bluray-4.dll', 'share\java\libbluray-j2se-1.5.0.jar', 'share\java\libbluray-awt-j2se-1.5.0.jar')) {
    if (!(Test-Path -LiteralPath (Join-Path $runtime $path))) { throw "Missing runtime component: $path" }
}
Copy-Item -LiteralPath (Join-Path $gccRoot 'mpcbe_libs\lib64\libmingwex.a') -Destination (Join-Path $source 'lib64') -Force
Copy-Item -LiteralPath (Join-Path $gccRoot "lib\gcc\x86_64-w64-mingw32\$($gccVersion.Trim())\libgcc.a") -Destination (Join-Path $source 'lib64') -Force
$log = Join-Path $component 'diagnostics\logs\player-build.log'
New-Item -ItemType Directory -Path (Split-Path $log -Parent) -Force | Out-Null
$policy = if ($StandardProfile) { 'false' } else { 'true' }
$savedMsys = $env:MPCBE_MSYS
$savedMingw = $env:MPCBE_MINGW
Push-Location $source
try {
    $env:MPCBE_MSYS = $ToolchainRoot
    $env:MPCBE_MINGW = $gccRoot
    & $msbuild mpc-be.sln /m:4 /v:minimal /nologo "/p:Configuration=Release;Platform=x64;WindowsTargetPlatformVersion=$SdkVersion;BlurayPortableTest=$policy" *> $log
    if ($LASTEXITCODE) { Get-Content -LiteralPath $log -Tail 35; throw "Player build failed; see $log" }
    & $msbuild src\apps\mpcresources\mpcresources.vcxproj /m /v:minimal /nologo "/p:Configuration=Release Russian;Platform=x64;WindowsTargetPlatformVersion=$SdkVersion;SolutionDir=$source/" *>> $log
    if ($LASTEXITCODE) { Get-Content -LiteralPath $log -Tail 35; throw "Russian resources failed; see $log" }
} finally {
    Pop-Location
    $env:MPCBE_MSYS = $savedMsys
    $env:MPCBE_MINGW = $savedMingw
}
$target = Join-Path $component 'out\mpc-be-bluray-x64'
New-Item -ItemType Directory -Path $target,(Join-Path $target 'Lang'),(Join-Path $target 'licenses') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $source '_bin\mpc-be_x64\mpc-be64.exe') -Destination $target -Force
Copy-Item -LiteralPath (Join-Path $source '_bin\mpc-be_x64\Lang\mpcresources.ru.dll') -Destination (Join-Path $target 'Lang') -Force
Copy-Item -Path (Join-Path $runtime 'bin\*.dll') -Destination $target -Force
Copy-Item -Path (Join-Path $runtime 'share\java\*.jar') -Destination $target -Force
Copy-Item -Path (Join-Path $runtime 'licenses\*') -Destination (Join-Path $target 'licenses') -Recurse -Force
Copy-Item -LiteralPath (Join-Path $source 'LICENSE.txt') -Destination (Join-Path $target 'licenses\MPC-BE-License.txt') -Force
Copy-Item -LiteralPath (Join-Path $component 'patches') -Destination $target -Recurse -Force
Copy-Item -LiteralPath (Join-Path $component 'assets\bdj-canvas.mkv') -Destination $target -Force
$ini = Join-Path $target 'mpc-be64.ini'
if (!(Test-Path -LiteralPath $ini)) {
    $seed = "[Settings]`r`nBluRayMenus=1`r`nChapterMarker=1`r`nMultipleInstances=2`r`nKeepHistory=0`r`nRememberFilePos=0`r`n[OSD]`r`nShowOSD=5`r`n[Audio]`r`nVolume=25`r`n[WebServer]`r`nEnableWebServer=0`r`n[Video]`r`nVideoRenderer=7`r`n"
    if (!$StandardProfile) { $seed += "[PortableTest]`r`nFirstRunComplete=0`r`n" }
    [IO.File]::WriteAllText($ini, $seed, [Text.Encoding]::Unicode)
}
$versions = Get-Content -LiteralPath (Join-Path $component 'versions.json') -Raw | ConvertFrom-Json
$sourceCommit = (& git -C $source rev-parse HEAD).Trim()
$revisionHeader = Get-Content -LiteralPath (Join-Path $source 'revision.h') -Raw
if ($revisionHeader -notmatch '#define REV_HASH "([0-9a-f]{7,40})"' -or !$sourceCommit.StartsWith($Matches[1])) {
    throw 'Generated revision.h does not identify the current source commit.'
}
$forkVersion = "$($versions.mpc_be.version)-bluray.$($versions.bluray_revision)"
$exeVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo((Join-Path $target 'mpc-be64.exe'))
$resourceVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo((Join-Path $target 'Lang\mpcresources.ru.dll'))
if ($exeVersion.ProductVersion -ne $forkVersion -or $exeVersion.ProductName -ne 'MPC-BE Blu-ray x64') {
    throw 'EXE branding differs from versions.json; check include/BlurayVersion.h.'
}
if ($resourceVersion.FileVersion -ne $exeVersion.FileVersion) {
    throw 'Russian resources and player have different numeric versions.'
}
$manifest = [ordered]@{
    upstream = $versions.mpc_be
    fork_version = $forkVersion
    source_commit = $sourceCommit
    source_dirty = [bool](& git -C $source status --porcelain --untracked-files=normal)
    bluray_revision = $versions.bluray_revision
    portable_test = !$StandardProfile
    files = @(@(Get-ChildItem -LiteralPath $target -File) + @(Get-ChildItem -LiteralPath (Join-Path $target 'Lang') -File) | Where-Object Extension -In '.exe','.dll','.jar','.mkv' | ForEach-Object {
        @{ name = $_.FullName.Substring($target.Length + 1).Replace('\','/'); sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
    })
}
$manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $target 'build-manifest.json') -Encoding utf8
Write-Output "Built: $target\mpc-be64.exe"
