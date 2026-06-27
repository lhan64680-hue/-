function Get-CineVaultRepoRoot {
    return Split-Path -Parent $PSScriptRoot
}

function Get-VisualStudioInstallationPath {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        $fallbackPaths = @(
            "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools",
            "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools"
        )
        foreach ($fallbackPath in $fallbackPaths) {
            if (Test-Path (Join-Path $fallbackPath "VC\Auxiliary\Build\vcvars64.bat")) {
                return $fallbackPath
            }
        }
        throw "vswhere.exe was not found and no fallback Visual Studio Build Tools path was detected."
    }

    $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($installPath)) {
        return $installPath.Trim()
    }

    $fallbackPaths = @(
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools"
    )
    foreach ($fallbackPath in $fallbackPaths) {
        if (Test-Path (Join-Path $fallbackPath "VC\Auxiliary\Build\vcvars64.bat")) {
            return $fallbackPath
        }
    }

    throw "No Visual Studio installation with MSVC C++ tools was found."
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
        "C:\Qt\6.6.3\msvc2019_64",
        "C:\Qt\6.7.3\msvc2019_64",
        "G:\Qt\6.8.0\msvc2022_64",
        "G:\Qt\6.7.3\msvc2022_64",
        "G:\Qt\6.6.3\msvc2022_64",
        "G:\Qt\6.6.3\msvc2019_64",
        "G:\Qt\6.7.3\msvc2019_64"
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

function Get-WindeployQtPath {
    param([string]$QtRoot)

    $resolvedQtRoot = Resolve-QtRoot -QtRoot $QtRoot
    $windeployqt = Join-Path $resolvedQtRoot "bin\windeployqt.exe"
    if (-not (Test-Path $windeployqt)) {
        throw "windeployqt.exe was not found under $resolvedQtRoot\bin."
    }

    return $windeployqt
}

function Get-InnoSetupCompilerPath {
    $command = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($null -ne $command -and -not [string]::IsNullOrWhiteSpace($command.Source)) {
        return $command.Source
    }

    $candidates = @(
        "C:\ProgramData\chocolatey\bin\ISCC.exe",
        (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\ISCC.exe"),
        (Join-Path $env:ProgramFiles "Inno Setup 6\ISCC.exe")
    )

    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    throw "Inno Setup compiler was not found. Install Inno Setup 6 or ensure ISCC.exe is available on PATH."
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
    $resolvedInnoSetupCompiler = $null
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

    try {
        $resolvedInnoSetupCompiler = Get-InnoSetupCompilerPath
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
        WindeployQt = (Get-WindeployQtPath -QtRoot $resolvedQtRoot)
        InnoSetupCompiler = $resolvedInnoSetupCompiler
    }
}
