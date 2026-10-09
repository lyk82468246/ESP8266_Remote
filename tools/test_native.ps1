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
& "$testOutput\contract.exe"
if ($LASTEXITCODE -ne 0) { throw 'Native contract tests failed.' }
