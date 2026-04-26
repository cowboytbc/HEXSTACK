param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'

$deliverablesRoot = Join-Path $ProjectRoot 'DELIVERABLES\Windows'
$releaseRoot = Join-Path $ProjectRoot 'RELEASES'
$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$bundleRoot = Join-Path $releaseRoot ("HEXSTACK-Windows-" + $timestamp)

New-Item -ItemType Directory -Force -Path $bundleRoot | Out-Null

$pathsToCopy = @(
    @{ Source = Join-Path $deliverablesRoot 'VST3';        Dest = Join-Path $bundleRoot 'VST3' },
    @{ Source = Join-Path $deliverablesRoot 'VST2';        Dest = Join-Path $bundleRoot 'VST2' },
    @{ Source = Join-Path $deliverablesRoot 'Standalone';  Dest = Join-Path $bundleRoot 'Standalone' },
    @{ Source = Join-Path $deliverablesRoot 'ZIP';         Dest = Join-Path $bundleRoot 'ZIP' }
)

foreach ($entry in $pathsToCopy)
{
    if (Test-Path $entry.Source)
    {
        Copy-Item -Recurse -Force $entry.Source $entry.Dest
    }
}

$bundleZip = Join-Path $releaseRoot ("HEXSTACK-Windows-" + $timestamp + '.zip')
if (Test-Path $bundleZip)
{
    Remove-Item -Force $bundleZip
}

Push-Location $bundleRoot
try
{
    Compress-Archive -Path * -DestinationPath $bundleZip -CompressionLevel Optimal
}
finally
{
    Pop-Location
}

Write-Output "Bundle folder: $bundleRoot"
Write-Output "Bundle zip:    $bundleZip"
