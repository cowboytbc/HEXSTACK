param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'

$source = Join-Path $ProjectRoot 'DELIVERABLES\Windows\VST3\HEXSTACK.vst3'
$destinationRoot = 'C:\Program Files\Common Files\VST3'
$destination = Join-Path $destinationRoot 'HEXSTACK.vst3'

if (-not (Test-Path $source))
{
    throw "VST3 deliverable not found at: $source"
}

try
{
    if (Test-Path $destination)
    {
        Remove-Item -Recurse -Force $destination
    }

    Copy-Item -Recurse -Force $source $destinationRoot
    Write-Output "Installed to $destination"
}
catch
{
    throw "Could not copy to $destinationRoot. Run this script from an elevated terminal if you want to install into the system VST3 folder."
}
