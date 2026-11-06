$ErrorActionPreference = 'Stop'
foreach ($relative in @('AirAlertMap.exe','WebView2Loader.dll','web\index.html','web\app.js','web\territories.js','data\regions.json')) {
    if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot $relative) -PathType Leaf)) {
        Write-Host "Missing portable file: $relative. Extract the entire ZIP again."
        exit 1
    }
}
$runtimeFound = $false
foreach ($hive in @([Microsoft.Win32.RegistryHive]::CurrentUser,[Microsoft.Win32.RegistryHive]::LocalMachine)) {
    foreach ($view in @([Microsoft.Win32.RegistryView]::Registry32,[Microsoft.Win32.RegistryView]::Registry64)) {
        $root=$null; $clients=$null
        try {
            $root=[Microsoft.Win32.RegistryKey]::OpenBaseKey($hive,$view)
            $clients=$root.OpenSubKey('Software\Microsoft\EdgeUpdate\Clients')
            if ($clients) {
                foreach ($name in $clients.GetSubKeyNames()) {
                    $client=$clients.OpenSubKey($name)
                    try {
                        $product=[string]$client.GetValue('name')
                        $version=[string]$client.GetValue('pv')
                        if ($product -like '*WebView2*' -and $version -match '^\d+\.' -and $version -ne '0.0.0.0') { $runtimeFound=$true }
                    } finally { if ($client) { $client.Dispose() } }
                }
            }
        } catch { } finally { if ($clients) { $clients.Dispose() }; if ($root) { $root.Dispose() } }
    }
}
# A runtime may exist before its updater registration is visible to this user.
foreach ($base in @(${env:ProgramFiles(x86)},$env:ProgramFiles,$env:LOCALAPPDATA)) {
    if ($base) {
        $application=Join-Path $base 'Microsoft\EdgeWebView\Application'
        if (Test-Path -LiteralPath $application -PathType Container) {
            foreach ($folder in Get-ChildItem -LiteralPath $application -Directory) {
                if (Test-Path -LiteralPath (Join-Path $folder.FullName 'msedgewebview2.exe') -PathType Leaf) { $runtimeFound=$true }
            }
        }
    }
}
if (-not $runtimeFound) {
    Write-Host 'Microsoft Edge WebView2 Runtime was not found. It is required for the map.'
    Write-Host 'Download the Evergreen Runtime from Microsoft, then run start.cmd again:'
    Write-Host 'https://developer.microsoft.com/microsoft-edge/webview2/#download-section'
    exit 2
}
$pythonFound=$false
foreach ($command in @('py','python')) {
    if (Get-Command $command -ErrorAction SilentlyContinue) {
        try {
            $arguments=@('-c','import sys; sys.exit(0 if sys.version_info >= (3,10) else 1)')
            if ($command -eq 'py') { $arguments=@('-3') + $arguments }
            & $command @arguments 2>$null | Out-Null
            if ($LASTEXITCODE -eq 0) { $pythonFound=$true; break }
        } catch { }
    }
}
if (-not $pythonFound) {
    Write-Host 'Python 3.10+ is optional: history reports need it; the map can still start.'
    Write-Host 'Python download: https://www.python.org/downloads/windows/'
}
exit 0
