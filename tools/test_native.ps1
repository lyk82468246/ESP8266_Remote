param(
    [string]$Compiler = "$PSScriptRoot\..\.pio-core\packages\toolchain-gccmingw32\bin\g++.exe"
)
$ErrorActionPreference = 'Stop'
$compilerBin = Split-Path -Parent (Resolve-Path -LiteralPath $Compiler).Path
$env:PATH = "$compilerBin;$env:PATH"
$projectRoot = (Resolve-Path -LiteralPath "$PSScriptRoot\..").Path
$testOutput = Join-Path $projectRoot '.tools\tests'
New-Item -ItemType Directory -Force -Path $testOutput | Out-Null
$jsonInclude = Join-Path $projectRoot '.pio\libdeps\nodemcuv2\ArduinoJson\src'
if (!(Test-Path -LiteralPath $jsonInclude)) { throw 'Run pio run first to install ArduinoJson.' }
& $Compiler -std=c++11 -Wall -Wextra -Werror "-I$projectRoot\tests\stubs" "-I$projectRoot\ESP8266_Remote" "-I$jsonInclude" "$projectRoot\tests\contract.cpp" -o "$testOutput\contract.exe"
if ($LASTEXITCODE -ne 0) { throw 'Native test compilation failed.' }
& "$testOutput\contract.exe" $testOutput
if ($LASTEXITCODE -ne 0) { throw 'Native contract tests failed.' }
foreach ($mode in @('sta', 'ap', 'empty')) {
    [string[]]$modeFlags = if ($mode -eq 'ap') { @('-DREMOTE_USE_STA=0') } else { @('-DREMOTE_USE_STA=1') }
    if ($mode -eq 'empty') { $modeFlags += '-DREMOTE_TEST_EMPTY_SSID=1' }
    & $Compiler -std=c++11 -Wall -Wextra -Werror @modeFlags "-I$projectRoot\tests\stubs" "-I$projectRoot\ESP8266_Remote" "$projectRoot\tests\network.cpp" -o "$testOutput\network-$mode.exe"
    if ($LASTEXITCODE -ne 0) { throw "Network $mode compilation failed." }
    & "$testOutput\network-$mode.exe"
    if ($LASTEXITCODE -ne 0) { throw "Network $mode tests failed." }
}
