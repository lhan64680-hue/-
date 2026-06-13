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

$version = Get-NextDistVersion -DistRoot (Join-Path $context.RepoRoot "dist")
$distDir = Join-Path $context.RepoRoot "dist\$version"
New-Item -ItemType Directory -Force -Path $distDir | Out-Null

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

Copy-Item $exePath -Destination $distDir -Force
$deployedExe = Join-Path $distDir "CineVault.exe"
$deployMode = if ($Configuration -ieq "Debug") { "--debug" } else { "--release" }
Invoke-VcVarsCommand "`"$($context.WindeployQt)`" $deployMode --qmldir `"$projectRoot\src\ui\qml`" `"$deployedExe`""

Write-Host $distDir
