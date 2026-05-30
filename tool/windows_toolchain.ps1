function Get-CineVaultRepoRoot {
    return Split-Path -Parent $PSScriptRoot
}

function Get-VisualStudioInstallationPath {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe was not found. Install Visual Studio 2022 Build Tools or newer."
    }

    $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($installPath)) {
        throw "No Visual Studio installation with MSVC C++ tools was found."
    }

    return $installPath.Trim()
}

function Get-VcVars64Path {
    $installPath = Get-VisualStudioInstallationPath
    $vcvarsPath = Join-Path $installPath "VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path $vcvarsPath)) {
        throw "vcvars64.bat was not found: $vcvarsPath"
    }

    return $vcvarsPath
}

function Resolve-QtRoot {
    param([string]$QtRoot)

    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($QtRoot)) {
        $candidates += $QtRoot
    }
    if (-not [string]::IsNullOrWhiteSpace($env:QT_ROOT)) {
        $candidates += $env:QT_ROOT
    }
    $candidates += @(
        "C:\Qt\6.8.0\msvc2022_64",
        "C:\Qt\6.7.3\msvc2022_64",
        "C:\Qt\6.6.3\msvc2022_64",
        "G:\Qt\6.8.0\msvc2022_64",
        "G:\Qt\6.7.3\msvc2022_64",
        "G:\Qt\6.6.3\msvc2022_64"
    )

    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }
        $qtConfig = Join-Path $candidate "lib\cmake\Qt6\Qt6Config.cmake"
        if (Test-Path $qtConfig) {
            return $candidate
        }
    }

    throw "Qt 6 MSVC kit was not found. Set QT_ROOT to a directory containing lib\cmake\Qt6\Qt6Config.cmake."
}

function Resolve-FfmpegDevRoot {
    param(
        [string]$FfmpegDevRoot,
        [switch]$Required
    )

    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($FfmpegDevRoot)) {
        $candidates += $FfmpegDevRoot
    }
    if (-not [string]::IsNullOrWhiteSpace($env:FFMPEG_DEV_ROOT)) {
        $candidates += $env:FFMPEG_DEV_ROOT
    }
    $candidates += @(
        "G:\data\app\DIT\ffmpeg-dev",
        "G:\data\app\DIT\ffmpeg_sdk",
        "C:\ffmpeg-dev"
    )

    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }
        $includeDir = Join-Path $candidate "include\libavformat\avformat.h"
        $libDir = Join-Path $candidate "lib"
        $binDir = Join-Path $candidate "bin"
        if ((Test-Path $includeDir) -and (Test-Path $libDir) -and (Test-Path $binDir)) {
            return $candidate
        }
    }

    if ($Required) {
        throw "FFmpeg development package was not found. Set FFMPEG_DEV_ROOT to a directory containing include, lib, and bin."
    }

    return $null
}

function Invoke-VcVarsCommand {
    param(
        [string]$CommandLine
    )

    $vcvarsPath = Get-VcVars64Path
    $fullCommand = "call `"$vcvarsPath`" >nul && $CommandLine"
    & cmd.exe /d /s /c $fullCommand
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed: $CommandLine"
    }
}

function Assert-DeveloperTools {
    Invoke-VcVarsCommand "where cl.exe >nul && where rc.exe >nul && where cmake.exe >nul && where ninja.exe >nul"
}

function Get-CineVaultBuildContext {
    param(
        [string]$QtRoot,
        [string]$FfmpegDevRoot,
        [switch]$RequireFfmpeg
    )

    $resolvedQtRoot = $null
    $resolvedFfmpegDevRoot = $null
    $errors = New-Object System.Collections.Generic.List[string]

    try {
        Assert-DeveloperTools
    } catch {
        $errors.Add($_.Exception.Message)
    }

    try {
        $resolvedQtRoot = Resolve-QtRoot -QtRoot $QtRoot
    } catch {
        $errors.Add($_.Exception.Message)
    }

    try {
        $resolvedFfmpegDevRoot = Resolve-FfmpegDevRoot -FfmpegDevRoot $FfmpegDevRoot -Required:$RequireFfmpeg
    } catch {
        $errors.Add($_.Exception.Message)
    }

    if ($errors.Count -gt 0) {
        throw ($errors -join [Environment]::NewLine)
    }

    return [PSCustomObject]@{
        RepoRoot = Get-CineVaultRepoRoot
        QtRoot = $resolvedQtRoot
        FfmpegDevRoot = $resolvedFfmpegDevRoot
        HasFfmpeg = -not [string]::IsNullOrWhiteSpace($resolvedFfmpegDevRoot)
    }
}
