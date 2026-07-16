[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$InstallerPath,

    [Parameter(Mandatory = $true)]
    [string]$InstallRoot,

    [switch]$AllowInstallerRegistration,

    [ValidateRange(30, 900)]
    [int]$InstallTimeoutSeconds = 600
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if (-not $AllowInstallerRegistration) {
    throw "Installer startup testing writes a temporary uninstall registration. Pass -AllowInstallerRegistration only on a disposable CI runner."
}

$resolvedInstaller = (Resolve-Path -LiteralPath $InstallerPath -ErrorAction Stop).Path
$temporaryRoot = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\')
$resolvedInstallRoot = [System.IO.Path]::GetFullPath($InstallRoot).TrimEnd('\')
$temporaryPrefix = $temporaryRoot + '\'
if (-not $resolvedInstallRoot.StartsWith($temporaryPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Installer probe root must stay below the system temporary directory: $resolvedInstallRoot"
}

if (Test-Path -LiteralPath $resolvedInstallRoot) {
    Remove-Item -LiteralPath $resolvedInstallRoot -Recurse -Force
}

$installerLog = "$resolvedInstallRoot-installer.log"
if (Test-Path -LiteralPath $installerLog) {
    Remove-Item -LiteralPath $installerLog -Force
}

$arguments = @(
    "/SP-",
    "/VERYSILENT",
    "/SUPPRESSMSGBOXES",
    "/NORESTART",
    "/NOCANCEL",
    "/CLOSEAPPLICATIONS",
    "/FORCECLOSEAPPLICATIONS",
    "/DIR=`"$resolvedInstallRoot`"",
    "/LOG=`"$installerLog`""
)

$installer = Start-Process `
    -FilePath $resolvedInstaller `
    -ArgumentList $arguments `
    -WindowStyle Hidden `
    -PassThru

if (-not $installer.WaitForExit($InstallTimeoutSeconds * 1000)) {
    try {
        $installer.Kill($true)
        $installer.WaitForExit()
    } catch {
        Write-Warning "Failed to stop timed-out installer probe: $($_.Exception.Message)"
    }
    throw "Installer startup probe timed out after $InstallTimeoutSeconds seconds. Log: $installerLog"
}

if ($installer.ExitCode -ne 0) {
    throw "Installer startup probe failed with exit code $($installer.ExitCode). Log: $installerLog"
}

$applicationPath = Join-Path $resolvedInstallRoot "CineVault.exe"
& (Join-Path $PSScriptRoot "test_cinevault_startup.ps1") -ApplicationPath $applicationPath
if ($LASTEXITCODE -ne 0) {
    throw "Installed CineVault startup probe failed. Log: $installerLog"
}

Remove-Item -LiteralPath $resolvedInstallRoot -Recurse -Force
Remove-Item -LiteralPath $installerLog -Force
Write-Host "Installed CineVault startup probe passed."
