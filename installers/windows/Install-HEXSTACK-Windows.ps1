param()

$ErrorActionPreference = 'Stop'

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$vst3Source = Join-Path $scriptRoot 'VST3\HEXSTACK.vst3'
$standaloneSource = Join-Path $scriptRoot 'Standalone\HEXSTACK.exe'

function Read-Choice {
    param(
        [string]$Prompt,
        [string[]]$AllowedValues
    )

    while ($true)
    {
        $value = (Read-Host $Prompt).Trim()
        if ($AllowedValues -contains $value)
        {
            return $value
        }

        Write-Host "Please enter one of: $($AllowedValues -join ', ')" -ForegroundColor Yellow
    }
}

function Read-InstallPath {
    param(
        [string]$Label,
        [string]$DefaultPath
    )

    Write-Host ""
    Write-Host "$Label install location:" -ForegroundColor Cyan
    Write-Host "1) Default: $DefaultPath"
    Write-Host "2) Choose my own folder"

    $mode = Read-Choice -Prompt 'Pick 1 or 2' -AllowedValues @('1', '2')
    if ($mode -eq '1')
    {
        return $DefaultPath
    }

    while ($true)
    {
        $custom = (Read-Host 'Enter the full destination folder path').Trim().Trim('"')
        if (-not [string]::IsNullOrWhiteSpace($custom))
        {
            return $custom
        }

        Write-Host 'Please enter a real folder path.' -ForegroundColor Yellow
    }
}

function Install-Vst3 {
    param([string]$Source)

    if (-not (Test-Path $Source))
    {
        throw "VST3 source not found at $Source"
    }

    $defaultPath = Join-Path $env:LOCALAPPDATA 'Programs\Common\VST3'
    $destinationRoot = Read-InstallPath -Label 'VST3 plugin' -DefaultPath $defaultPath

    New-Item -ItemType Directory -Force -Path $destinationRoot | Out-Null

    $destination = Join-Path $destinationRoot 'HEXSTACK.vst3'
    if (Test-Path $destination)
    {
        Remove-Item -Recurse -Force $destination
    }

    Copy-Item -Recurse -Force $Source $destination
    Write-Host "Installed VST3 to $destination" -ForegroundColor Green
}

function Install-Standalone {
    param([string]$Source)

    if (-not (Test-Path $Source))
    {
        throw "Standalone source not found at $Source"
    }

    $defaultPath = Join-Path $env:LOCALAPPDATA 'Programs\HEXSTACK'
    $destinationRoot = Read-InstallPath -Label 'Standalone app' -DefaultPath $defaultPath

    New-Item -ItemType Directory -Force -Path $destinationRoot | Out-Null

    $destination = Join-Path $destinationRoot 'HEXSTACK.exe'
    Copy-Item -Force $Source $destination
    Write-Host "Installed Standalone app to $destination" -ForegroundColor Green
}

Write-Host 'HEXSTACK Windows Installer Helper' -ForegroundColor Cyan
Write-Host 'This helper installs the files that came in this package.'

$installVst3 = Read-Choice -Prompt 'Install the VST3 plugin? (y/n)' -AllowedValues @('y', 'n')
if ($installVst3 -eq 'y')
{
    Install-Vst3 -Source $vst3Source
}

$installStandalone = Read-Choice -Prompt 'Install the standalone app? (y/n)' -AllowedValues @('y', 'n')
if ($installStandalone -eq 'y')
{
    Install-Standalone -Source $standaloneSource
}

Write-Host ''
Write-Host 'Done. If your DAW was open, close it and rescan plugins.' -ForegroundColor Cyan