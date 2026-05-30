param(
    [string]$QtRoot = $env:QT_ROOT,
    [string]$Generator = "Ninja",
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
. "$PSScriptRoot\version_helpers.ps1"

$ProjectRoot = Join-Path $RepoRoot "dit-tools-src\cinevault-pro"
$BuildDir = Join-Path $ProjectRoot "build\$Configuration"

if (-not (Test-Path $QtRoot)) {
    throw "未找到 Qt 根目录，请设置 QT_ROOT 环境变量。"
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$version = Get-NextDistVersion -DistRoot (Join-Path $RepoRoot "dist")
$distDir = Join-Path $RepoRoot "dist\$version"
New-Item -ItemType Directory -Force -Path $distDir | Out-Null

cmake -S $ProjectRoot -B $BuildDir -G $Generator -DCMAKE_BUILD_TYPE=$Configuration -DQT_ROOT="$QtRoot"
cmake --build $BuildDir --config $Configuration

$binaryCandidates = @(
    (Join-Path $BuildDir "src\app\CineVault.exe"),
    (Join-Path $BuildDir "CineVault.exe")
)

foreach ($candidate in $binaryCandidates) {
    if (Test-Path $candidate) {
        Copy-Item $candidate -Destination $distDir -Force
    }
}

Write-Host $distDir
