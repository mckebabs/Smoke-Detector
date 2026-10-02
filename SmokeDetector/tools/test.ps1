param(
    [string]$Compiler = 'g++',
    [string]$WorkRoot = (Join-Path $env:TEMP 'smoke-detector-tests'),
    [switch]$Zig
)
$ErrorActionPreference = 'Stop'
$taskSource = Split-Path -Parent $PSScriptRoot
New-Item -ItemType Directory -Force -Path $WorkRoot | Out-Null
$taskBinary = Join-Path ([IO.Path]::GetFullPath($WorkRoot)) 'detector-tests.exe'
$taskArguments = @('-std=c++11','-Wall','-Wextra','-Werror','-pedantic','-I', $taskSource,
    (Join-Path $taskSource 'Detector.cpp'), (Join-Path $taskSource 'tests/detector_tests.cpp'), '-o', $taskBinary)
if ($Zig) {
    # The tested core uses only C headers; no C++ standard runtime is needed.
    & $Compiler c++ -nostdlib++ -fno-exceptions -fno-rtti @taskArguments
} else { & $Compiler @taskArguments }
if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed.' }
& $taskBinary
if ($LASTEXITCODE -ne 0) { throw 'Detector tests failed.' }
