param([Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'prepare-webview.ps1')
New-Item -ItemType Directory -Path $Destination -Force | Out-Null
foreach ($folder in @('web','analytics','data')) {
    New-Item -ItemType Directory -Path (Join-Path $Destination $folder) -Force | Out-Null
}
Get-ChildItem -LiteralPath (Join-Path $projectRoot 'web') -File | Copy-Item -Destination (Join-Path $Destination 'web') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'analytics\analyze.py') -Destination (Join-Path $Destination 'analytics') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'analytics\manage_history.py') -Destination (Join-Path $Destination 'analytics') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'data\regions.json') -Destination (Join-Path $Destination 'data') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'data\NOTICE.md') -Destination (Join-Path $Destination 'data') -Force
$loaderSource=Join-Path $projectRoot 'vendor\webview2\sdk\build\native\x86\WebView2Loader.dll'
$loaderTarget=Join-Path $Destination 'WebView2Loader.dll'
# Windows locks the loader while the application runs. An identical installed
# loader needs no write, which also allows refreshing local HTML assets.
if (-not (Test-Path -LiteralPath $loaderTarget) -or
    (Get-FileHash -LiteralPath $loaderSource).Hash -ne (Get-FileHash -LiteralPath $loaderTarget).Hash) {
    Copy-Item -LiteralPath $loaderSource -Destination $loaderTarget -Force
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'vendor\webview2\sdk\LICENSE.txt') -Destination (Join-Path $Destination 'WebView2-LICENSE.txt') -Force
