param(
    [string]$QtRoot = $env:QT_ROOT,
    [string]$FfmpegDevRoot = $env:FFMPEG_DEV_ROOT,
    [string]$Configuration = "Release",
    [switch]$RealWorkflow,
    [switch]$EnableFfmpeg
)

$ErrorActionPreference = "Stop"

. "$PSScriptRoot\windows_toolchain.ps1"
. "$PSScriptRoot\version_helpers.ps1"

if ($RealWorkflow -and $EnableFfmpeg) {
    throw "-RealWorkflow uses the real project/import workflow without FFmpeg. Use -EnableFfmpeg by itself for the FFmpeg build."
}

$context = Get-CineVaultBuildContext -QtRoot $QtRoot -FfmpegDevRoot $FfmpegDevRoot -RequireFfmpeg:$EnableFfmpeg
$projectRoot = Join-Path $context.RepoRoot "dit-tools-src\cinevault-pro"

$isDebug = $Configuration -ieq "Debug"
$configurePreset = if ($EnableFfmpeg) {
    if ($isDebug) { "windows-msvc-debug-ffmpeg" } else { "windows-msvc-release-ffmpeg" }
} elseif ($RealWorkflow) {
    if ($isDebug) { "windows-msvc-debug-real" } else { "windows-msvc-release-real" }
} else {
    if ($isDebug) { "windows-msvc-debug" } else { "windows-msvc-release" }
}
$buildPreset = $configurePreset
$buildDirName = $configurePreset
$buildDir = Join-Path $projectRoot "build\$buildDirName"

$env:QT_ROOT = $context.QtRoot
if ($context.HasFfmpeg) {
    $env:FFMPEG_DEV_ROOT = $context.FfmpegDevRoot
} elseif (Test-Path env:FFMPEG_DEV_ROOT) {
    Remove-Item env:FFMPEG_DEV_ROOT
}

Push-Location $projectRoot
try {
    Invoke-VcVarsCommand "cmake --preset $configurePreset"
    Invoke-VcVarsCommand "cmake --build --preset $buildPreset --config $Configuration"
} finally {
    Pop-Location
}

$outputRoot = Join-Path $context.RepoRoot "output"
$version = Get-NextDistVersion -DistRoot $outputRoot -ReferenceRoots @((Join-Path $context.RepoRoot "dist"))
$installerOutputDir = Join-Path $outputRoot $version
$stagingRoot = Join-Path $outputRoot "_installer_staging"
$stagingDir = Join-Path $stagingRoot $version

function Remove-InstallerStagingDir {
    param(
        [string]$Path,
        [string]$Root
    )

    $resolvedPath = [System.IO.Path]::GetFullPath($Path)
    $resolvedRoot = [System.IO.Path]::GetFullPath($Root)
    $rootPrefix = $resolvedRoot
    if (-not $rootPrefix.EndsWith([System.IO.Path]::DirectorySeparatorChar)) {
        $rootPrefix += [System.IO.Path]::DirectorySeparatorChar
    }

    if ($resolvedPath.Equals($resolvedRoot, [System.StringComparison]::OrdinalIgnoreCase) -or
        -not $resolvedPath.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove path outside installer staging root: $resolvedPath"
    }

    if (Test-Path $resolvedPath) {
        Remove-Item -LiteralPath $resolvedPath -Recurse -Force
    }
}

Remove-InstallerStagingDir -Path $stagingDir -Root $stagingRoot
New-Item -ItemType Directory -Force -Path $stagingDir, $installerOutputDir | Out-Null

$binaryCandidates = @(
    (Join-Path $buildDir "CineVault.exe"),
    (Join-Path $buildDir "src\app\CineVault.exe")
)

$exePath = $null
foreach ($candidate in $binaryCandidates) {
    if (Test-Path $candidate) {
        $exePath = $candidate
        break
    }
}

if ($null -eq $exePath) {
    throw "Build completed but CineVault.exe was not found."
}

Copy-Item $exePath -Destination $stagingDir -Force
$deployedExe = Join-Path $stagingDir "CineVault.exe"
$deployMode = if ($Configuration -ieq "Debug") { "--debug" } else { "--release" }
Invoke-VcVarsCommand "`"$($context.WindeployQt)`" $deployMode --qmldir `"$projectRoot\src\ui\qml`" `"$deployedExe`""

$installerScript = Join-Path $context.RepoRoot "installer\windows\cinevault.iss"
if (-not (Test-Path $installerScript)) {
    throw "Installer script was not found: $installerScript"
}

$appVersion = $version.TrimStart("v")
& $context.InnoSetupCompiler "/DAppVersion=$appVersion" "/DVersionTag=$version" "/DSourceDir=$stagingDir" "/DOutputDir=$installerOutputDir" $installerScript
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup installer build failed."
}

$installerPath = Join-Path $installerOutputDir "CineVault-Setup-$version.exe"
if (-not (Test-Path $installerPath)) {
    throw "Installer build completed but output was not found: $installerPath"
}

Remove-InstallerStagingDir -Path $stagingDir -Root $stagingRoot

Write-Host $installerPath
