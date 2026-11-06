param([string]$BdsRoot = $env:BDS, [string]$Destination = (Join-Path $PSScriptRoot '..\build\AirAlertMap.res'))
$ErrorActionPreference = 'Stop'
if (-not $BdsRoot) { $BdsRoot = 'C:\Program Files (x86)\Embarcadero\Studio\37.0' }
$compiler = Join-Path $BdsRoot 'bin\brcc32.exe'
if (-not (Test-Path -LiteralPath $compiler -PathType Leaf)) { throw 'brcc32.exe not found. Specify -BdsRoot.' }
$resourceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\resources'))
if (-not (Test-Path -LiteralPath (Join-Path $resourceRoot 'AirAlertMap.ico'))) { & (Join-Path $PSScriptRoot 'generate-icon.ps1') }
$output = [IO.Path]::GetFullPath($Destination)
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($output)) | Out-Null
Push-Location -LiteralPath $resourceRoot
try {
    & $compiler '-32' ("-fo" + $output) 'AirAlertMap.rc'
    if ($LASTEXITCODE -ne 0) { throw 'Application resource compilation failed.' }
} finally { Pop-Location }
Write-Host "Resources: $output"
