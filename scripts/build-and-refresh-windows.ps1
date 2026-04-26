param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$exitCode = 0

Push-Location $ProjectRoot
try
{
    cmake -S . -B build
    if ($LASTEXITCODE -ne 0)
    {
        $exitCode = $LASTEXITCODE
    }

    if ($exitCode -eq 0)
    {
        $vst3Bundle = Join-Path $ProjectRoot 'build\Hexstack_artefacts\Release\VST3\HEXSTACK.vst3'

        cmake --build build --config Release --target Hexstack_Standalone
        if ($LASTEXITCODE -ne 0)
        {
            $exitCode = $LASTEXITCODE
        }
    }

    if ($exitCode -eq 0)
    {
        cmake --build build --config Release --target Hexstack_VST3
        if ($LASTEXITCODE -ne 0)
        {
            if (Test-Path $vst3Bundle)
            {
                Write-Warning 'Hexstack_VST3 finished with a JUCE helper error, but the VST3 bundle exists. Continuing to refresh deliverables.'
                $global:LASTEXITCODE = 0
            }
            else
            {
                $exitCode = $LASTEXITCODE
            }
        }
    }

    if ($exitCode -eq 0 -and (Test-Path '.\build\Hexstack_VST.vcxproj'))
    {
        cmake --build build --config Release --target Hexstack_VST
        if ($LASTEXITCODE -ne 0)
        {
            Write-Warning 'Hexstack_VST did not build successfully. Refreshing deliverables with the formats currently available.'
            $global:LASTEXITCODE = 0
        }
    }

    if ($exitCode -eq 0)
    {
        & (Join-Path $ProjectRoot 'scripts\refresh-deliverables.ps1') -ProjectRoot $ProjectRoot
        if ($LASTEXITCODE -ne 0)
        {
            $exitCode = $LASTEXITCODE
        }
    }
}
finally
{
    Pop-Location
}

exit $exitCode
