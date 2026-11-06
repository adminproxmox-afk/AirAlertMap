param([switch]$TestsOnly, [string]$BdsRoot = $env:BDS)
$ErrorActionPreference = 'Stop'
if (-not $BdsRoot) { $BdsRoot = 'C:\Program Files (x86)\Embarcadero\Studio\37.0' }
$compiler = Join-Path $BdsRoot 'bin\bcc32.exe'
$assembler = Join-Path $BdsRoot 'bin\tasm32.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw 'C++Builder Win32 Classic compiler not found. Specify -BdsRoot.' }
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Path 'build' -Force | Out-Null
    & $assembler /ml /m2 'asm\AlertAsm.asm,build\AlertAsm.obj'
    if ($LASTEXITCODE -ne 0) { throw 'Assembly failed' }
    Copy-Item -LiteralPath 'src\MainForm.dfm' -Destination 'build\MainForm.dfm' -Force
    Set-Location -LiteralPath (Join-Path $PSScriptRoot 'build')
    & $compiler '-tC' '-tM' '-I..' '-eModelTests.exe' '..\tests\ModelTests.cpp' '..\src\AlertModel.cpp' 'AlertAsm.obj'
    if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed' }
    & '.\ModelTests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Model tests failed' }
    & $compiler '-tC' '-tM' '-I..' '-eFeedTests.exe' '..\tests\FeedTests.cpp' '..\src\AlertFeed.cpp'
    if ($LASTEXITCODE -ne 0) { throw 'Feed test compilation failed' }
    & '.\FeedTests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Feed tests failed' }
    & $compiler '-tCV' '-tM' '-I..' '-eHttpTests.exe' '..\tests\HttpTests.cpp' '..\src\AlertsHttp.cpp' '..\src\AlertFeed.cpp' '..\src\UkraineAlarmFeed.cpp' '..\src\MapData.cpp'
    if ($LASTEXITCODE -ne 0) { throw 'HTTP test compilation failed' }
    & '.\HttpTests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'HTTP tests failed' }
    & $compiler '-tCV' '-tM' '-I..' '-eUkraineAlarmTests.exe' '..\tests\UkraineAlarmTests.cpp' '..\src\UkraineAlarmFeed.cpp' '..\src\AlertFeed.cpp' '..\src\MapData.cpp'
    if ($LASTEXITCODE -ne 0) { throw 'UkraineAlarm test compilation failed' }
    & '.\UkraineAlarmTests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'UkraineAlarm tests failed' }
    & $compiler '-tC' '-tM' '-I..' '-eCredentialTests.exe' '..\tests\CredentialTests.cpp' '..\src\ApiCredentials.cpp' '..\src\AlertFeed.cpp'
    if ($LASTEXITCODE -ne 0) { throw 'Credential test compilation failed' }
    & '.\CredentialTests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Credential tests failed' }
    & $compiler '-tCV' '-tM' '-I..' '-eStorageTests.exe' '..\tests\StorageTests.cpp' '..\src\AppStorage.cpp' '..\src\Preferences.cpp' '..\src\AlertsHttp.cpp' '..\src\UkraineAlarmFeed.cpp' '..\src\AlertFeed.cpp' '..\src\MapData.cpp'
    if ($LASTEXITCODE -ne 0) { throw 'Storage test compilation failed' }
    & '.\StorageTests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Storage tests failed' }
    & $compiler '-tCV' '-tM' '-I..' '-ePreferencesTests.exe' '..\tests\PreferencesTests.cpp' '..\src\Preferences.cpp'
    if ($LASTEXITCODE -ne 0) { throw 'Preferences test compilation failed' }
    & '.\PreferencesTests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Preferences tests failed' }
    & $compiler '-tCV' '-tM' '-I..' '-eUiCommandTests.exe' '..\tests\UiCommandTests.cpp' '..\src\UiCommand.cpp' '..\src\Preferences.cpp' '..\src\AlertFeed.cpp'
    if ($LASTEXITCODE -ne 0) { throw 'UI command test compilation failed' }
    & '.\UiCommandTests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'UI command tests failed' }
    & (Join-Path $PSScriptRoot 'tools\prepare-resources.ps1') -BdsRoot $BdsRoot
    & $compiler '-tCV' '-tM' '-I..' '-eMainWindowTests.exe' '..\tests\MainWindowTests.cpp' '..\src\WebMapForm.cpp' 'AirAlertMap.res'
    if ($LASTEXITCODE -ne 0) { throw 'Main window test compilation failed' }
    & '.\MainWindowTests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Main window registration test failed' }
    if (-not $TestsOnly) {
        & $compiler '-tWVU' '-tM' '-DUNICODE' '-D_UNICODE' '-I..' '-eAirAlertMap.exe' '..\src\AirAlertMap.cpp' '..\src\MainForm.cpp' '..\src\AlertModel.cpp' '..\src\MapData.cpp' '..\src\AlertFeed.cpp' '..\src\AlertsHttp.cpp' '..\src\UkraineAlarmFeed.cpp' '..\src\ApiCredentials.cpp' '..\src\AppStorage.cpp' '..\src\Preferences.cpp' '..\src\WebMapForm.cpp' '..\src\HistoryReport.cpp' '..\src\UiCommand.cpp' 'AlertAsm.obj' 'AirAlertMap.res'
        if ($LASTEXITCODE -ne 0) { throw 'Application build failed' }
        & (Join-Path $PSScriptRoot 'tools\package-assets.ps1') -Destination (Join-Path $PSScriptRoot 'build')
        Write-Host "Application: $PSScriptRoot\build\AirAlertMap.exe"
    }
} finally { Pop-Location }
