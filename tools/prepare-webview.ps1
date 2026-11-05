param([string]$VendorRoot = (Join-Path $PSScriptRoot '..\vendor\webview2'))
$ErrorActionPreference = 'Stop'
$loader = Join-Path $VendorRoot 'sdk\build\native\x86\WebView2Loader.dll'
if (-not (Test-Path -LiteralPath $loader)) {
    New-Item -ItemType Directory -Path $VendorRoot -Force | Out-Null
    $archive = Join-Path $VendorRoot 'sdk.zip'
    Invoke-WebRequest -Uri 'https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/1.0.2210.55/microsoft.web.webview2.1.0.2210.55.nupkg' -OutFile $archive
    $expectedHash = 'C0945DDFE38272F10F24673A1CB07B54B018F614EEC85D3E830628369039AD3D'
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expectedHash) { throw 'WebView2 SDK archive checksum mismatch.' }
    Expand-Archive -LiteralPath $archive -DestinationPath (Join-Path $VendorRoot 'sdk') -Force
}
if (-not (Test-Path -LiteralPath $loader)) { throw 'WebView2 x86 loader is missing.' }
