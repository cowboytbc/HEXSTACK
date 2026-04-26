param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'

function Repair-JsonFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path $Path)) {
        return
    }

    $raw = Get-Content -Path $Path -Raw -Encoding UTF8
    if ([string]::IsNullOrWhiteSpace($raw)) {
        return
    }

    $sanitized = [System.Text.RegularExpressions.Regex]::Replace($raw, ',(?=\s*[}\]])', '')

    try {
        $jsonObject = $sanitized | ConvertFrom-Json
        $normalized = $jsonObject | ConvertTo-Json -Depth 100
        [System.IO.File]::WriteAllText($Path, $normalized + [Environment]::NewLine, [System.Text.UTF8Encoding]::new($false))
    }
    catch {
        Write-Warning "Could not sanitize JSON file: $Path"
        throw
    }
}

$buildRelease = Join-Path $ProjectRoot 'build\Hexstack_artefacts\Release'
$deliverablesRoot = Join-Path $ProjectRoot 'DELIVERABLES\Windows'
$vst3Source = Join-Path $buildRelease 'VST3\HEXSTACK.vst3'
$vst2Source = Join-Path $buildRelease 'VST\HEXSTACK.dll'
$standaloneSource = Join-Path $buildRelease 'Standalone\HEXSTACK.exe'

$vst3DestRoot = Join-Path $deliverablesRoot 'VST3'
$vst2DestRoot = Join-Path $deliverablesRoot 'VST2'
$standaloneDestRoot = Join-Path $deliverablesRoot 'Standalone'
$zipRoot = Join-Path $deliverablesRoot 'ZIP'

New-Item -ItemType Directory -Force -Path $vst3DestRoot | Out-Null
New-Item -ItemType Directory -Force -Path $vst2DestRoot | Out-Null
New-Item -ItemType Directory -Force -Path $standaloneDestRoot | Out-Null
New-Item -ItemType Directory -Force -Path $zipRoot | Out-Null

$vst3Dest = Join-Path $vst3DestRoot 'HEXSTACK.vst3'
$vst2Dest = Join-Path $vst2DestRoot 'HEXSTACK.dll'
$standaloneDest = Join-Path $standaloneDestRoot 'HEXSTACK.exe'

if (Test-Path $vst3Source)
{
    Repair-JsonFile -Path (Join-Path $vst3Source 'Contents\Resources\moduleinfo.json')

    if (Test-Path $vst3Dest)
    {
        Remove-Item -Recurse -Force $vst3Dest
    }

    Copy-Item -Recurse -Force $vst3Source $vst3DestRoot
    Repair-JsonFile -Path (Join-Path $vst3Dest 'Contents\Resources\moduleinfo.json')
}

if (Test-Path $vst2Source)
{
    Copy-Item -Force $vst2Source $vst2Dest
}

if (Test-Path $standaloneSource)
{
    Copy-Item -Force $standaloneSource $standaloneDest
}

Get-ChildItem -Path $zipRoot -Filter '*.zip' -File -ErrorAction SilentlyContinue | Remove-Item -Force

Push-Location $deliverablesRoot
try
{
    if (Test-Path '.\VST3\HEXSTACK.vst3')
    {
        Compress-Archive -Path '.\VST3\HEXSTACK.vst3' -DestinationPath '.\ZIP\HEXSTACK-Windows-VST3.zip' -CompressionLevel Optimal
    }

    if (Test-Path '.\Standalone\HEXSTACK.exe')
    {
        Compress-Archive -Path '.\Standalone\HEXSTACK.exe' -DestinationPath '.\ZIP\HEXSTACK-Windows-Standalone.zip' -CompressionLevel Optimal
    }

    $allItems = @()

    if (Test-Path '.\VST3\HEXSTACK.vst3')
    {
        $allItems += '.\VST3\HEXSTACK.vst3'
    }

    if (Test-Path '.\VST2\HEXSTACK.dll')
    {
        $allItems += '.\VST2\HEXSTACK.dll'
    }

    if (Test-Path '.\Standalone\HEXSTACK.exe')
    {
        $allItems += '.\Standalone\HEXSTACK.exe'
    }

    if ($allItems.Count -gt 0)
    {
        Compress-Archive -Path $allItems -DestinationPath '.\ZIP\HEXSTACK-Windows-All.zip' -CompressionLevel Optimal
    }
}
finally
{
    Pop-Location
}

Get-ChildItem -Recurse $deliverablesRoot | Select-Object FullName
