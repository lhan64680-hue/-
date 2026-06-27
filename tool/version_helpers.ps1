function Get-NextDistVersion {
    param(
        [string]$DistRoot,
        [string[]]$ReferenceRoots = @()
    )

    $roots = @($DistRoot) + $ReferenceRoots
    $versions = @()

    foreach ($root in $roots) {
        if (-not (Test-Path $root)) {
            continue
        }

        $versions += Get-ChildItem -Path $root -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^v(\d+)\.(\d+)\.(\d+)$' } |
            ForEach-Object {
                [PSCustomObject]@{
                    Major = [int]$Matches[1]
                    Minor = [int]$Matches[2]
                    Patch = [int]$Matches[3]
                }
            }
    }

    $versions = $versions | Sort-Object Major, Minor, Patch

    if (-not $versions) {
        return "v0.1.0"
    }

    $latest = $versions[-1]
    return "v{0}.{1}.{2}" -f $latest.Major, $latest.Minor, ($latest.Patch + 1)
}
