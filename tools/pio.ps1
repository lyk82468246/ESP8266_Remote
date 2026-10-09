# Uses the project-local tools installed for this workspace. Arguments pass through to PIO.
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path -LiteralPath "$PSScriptRoot\..").Path
$localPythonModules = Join-Path $projectRoot '.tools\python'
if (!(Test-Path -LiteralPath "$localPythonModules\platformio")) {
    throw 'Install local tools: python -m pip install --target .tools/python platformio==6.1.18'
}
$env:PYTHONPATH = $localPythonModules
$env:PLATFORMIO_CORE_DIR = Join-Path $projectRoot '.pio-core'
Push-Location -LiteralPath $projectRoot
try {
    & python -m platformio @args
    if ($LASTEXITCODE -ne 0) { throw "PlatformIO failed: $LASTEXITCODE" }
} finally {
    Pop-Location
}
