param([string]$BdsRoot = $env:BDS)
$ErrorActionPreference = 'Stop'
if (-not $BdsRoot) { $BdsRoot = 'C:\Program Files (x86)\Embarcadero\Studio\37.0' }
$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    python -m unittest discover -s tests -p 'test_*.py'
    if ($LASTEXITCODE -ne 0) { throw 'Python analytics tests failed' }
    node --test tests/map-model.test.js tests/detail-model.test.js tests/geodata.test.js
    if ($LASTEXITCODE -ne 0) { throw 'Map JavaScript tests failed' }
    & (Join-Path $PSScriptRoot 'package-assets.ps1') -Destination (Join-Path $projectRoot 'build')
    Set-Location -LiteralPath (Join-Path $projectRoot 'build')
    & (Join-Path $BdsRoot 'bin\bcc32.exe') '-tCV' '-tM' '-I..' '-eReportTests.exe' '..\tests\ReportTests.cpp' '..\src\HistoryReport.cpp'
    if ($LASTEXITCODE -ne 0) { throw 'Report launch test compilation failed' }
    & '.\ReportTests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Native Python report launch test failed' }
} finally { Pop-Location }
