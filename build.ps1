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
    if (-not $TestsOnly) {
        & $compiler '-tWVU' '-tM' '-DUNICODE' '-D_UNICODE' '-I..' '-eAirAlertMap.exe' '..\src\AirAlertMap.cpp' '..\src\MainForm.cpp' '..\src\AlertModel.cpp' '..\src\MapData.cpp' 'AlertAsm.obj'
        if ($LASTEXITCODE -ne 0) { throw 'Application build failed' }
        Write-Host "Application: $PSScriptRoot\build\AirAlertMap.exe"
    }
} finally { Pop-Location }
