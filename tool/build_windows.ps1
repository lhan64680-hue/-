param(
    [string]$QtRoot = $env:QT_ROOT,
    [string]$FfmpegDevRoot = $env:FFMPEG_DEV_ROOT,
    [string]$Configuration = "Release",
    [switch]$EnableFfmpeg
)

$ErrorActionPreference = "Stop"

. "$PSScriptRoot\windows_toolchain.ps1"
. "$PSScriptRoot\version_helpers.ps1"

$context = Get-CineVaultBuildContext -QtRoot $QtRoot -FfmpegDevRoot $FfmpegDevRoot -RequireFfmpeg:$EnableFfmpeg
$projectRoot = Join-Path $context.RepoRoot "dit-tools-src\cinevault-pro"

$isDebug = $Configuration -ieq "Debug"
$configurePreset = if ($EnableFfmpeg) {
    if ($isDebug) { "windows-msvc-debug-ffmpeg" } else { "windows-msvc-release-ffmpeg" }
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

Invoke-VcVarsCommand "cmake --preset $configurePreset"
Invoke-VcVarsCommand "cmake --build --preset $buildPreset --config $Configuration"

$version = Get-NextDistVersion -DistRoot (Join-Path $context.RepoRoot "dist")
$distDir = Join-Path $context.RepoRoot "dist\$version"
New-Item -ItemType Directory -Force -Path $distDir | Out-Null

$binaryCandidates = @(
    (Join-Path $buildDir "CineVault.exe"),
    (Join-Path $buildDir "src\app\CineVault.exe")
)

$copied = $false
foreach ($candidate in $binaryCandidates) {
    if (Test-Path $candidate) {
        Copy-Item $candidate -Destination $distDir -Force
        $copied = $true
    }
}

if (-not $copied) {
    throw "Build completed but CineVault.exe was not found."
}

Write-Host $distDir
