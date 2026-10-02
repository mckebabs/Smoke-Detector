param(
    [string]$ArduinoCli = 'arduino-cli',
    [string]$WorkRoot = (Join-Path $env:TEMP 'smoke-detector-build'),
    [switch]$CheckOnly
)
$ErrorActionPreference = 'Stop'
$taskSource = Split-Path -Parent $PSScriptRoot
$taskVersion = (& $ArduinoCli version | Out-String)
if ($LASTEXITCODE -ne 0 -or $taskVersion -notmatch 'Version: 1\.3\.1\b') {
    throw 'This build is pinned to Arduino CLI 1.3.1. Pass its path with -ArduinoCli.'
}
$taskWork = [IO.Path]::GetFullPath($WorkRoot)
if ($taskWork -eq [IO.Path]::GetFullPath($taskSource)) {
    throw 'WorkRoot must differ from the source directory.'
}
if ($CheckOnly) { $taskWork = Join-Path $taskWork 'check' }
$taskSketch = Join-Path $taskWork 'SmokeDetector'
if ([IO.Path]::GetFullPath($taskSketch) -eq [IO.Path]::GetFullPath($taskSource)) {
    throw 'The staged sketch must not overwrite the source directory.'
}
$taskOutput = Join-Path $taskWork 'output'
$taskConfig = Join-Path $taskWork 'arduino-cli.yaml'
New-Item -ItemType Directory -Force -Path $taskSketch,$taskOutput | Out-Null
# Stage only firmware inputs; build outputs and dependency caches stay off-source.
foreach ($taskFile in Get-ChildItem -LiteralPath $taskSource -File) {
    if ($taskFile.Extension -in '.ino','.cpp','.h' -and $taskFile.Name -ne 'Secrets.h') {
        Copy-Item -LiteralPath $taskFile.FullName -Destination $taskSketch -Force
    }
}
Copy-Item -LiteralPath (Join-Path $taskSource 'sketch.yaml') -Destination $taskSketch -Force
if ($CheckOnly) {
    Copy-Item -LiteralPath (Join-Path $taskSource 'Secrets.example.h') -Destination (Join-Path $taskSketch 'Secrets.h') -Force
} else {
    $taskSecrets = Join-Path $taskSource 'Secrets.h'
    if (!(Test-Path -LiteralPath $taskSecrets)) { throw 'Create Secrets.h from Secrets.example.h first.' }
    if ((Get-Content -LiteralPath $taskSecrets -Raw) -match 'REPLACE_') {
        throw 'Replace every Secrets.h placeholder before building deployable firmware.'
    }
    Copy-Item -LiteralPath $taskSecrets -Destination (Join-Path $taskSketch 'Secrets.h') -Force
}
$taskData = (Join-Path $taskWork 'data').Replace('\','/')
$taskDownloads = (Join-Path $taskWork 'downloads').Replace('\','/')
$taskUser = (Join-Path $taskWork 'user').Replace('\','/')
@"
board_manager:
  additional_urls:
    - https://arduino.esp8266.com/stable/package_esp8266com_index.json
directories:
  data: '$taskData'
  downloads: '$taskDownloads'
  user: '$taskUser'
"@ | Set-Content -LiteralPath $taskConfig -Encoding utf8
& $ArduinoCli compile --config-file $taskConfig --profile nodemcu --warnings all --output-dir $taskOutput $taskSketch
if ($LASTEXITCODE -ne 0) { throw 'Firmware compilation failed.' }
if ($CheckOnly) {
    Write-Output 'CHECK BUILD ONLY: placeholder credentials; do not upload this binary.'
}
Write-Output "Firmware output: $taskOutput"
Write-Output "Staged sketch: $taskSketch"
