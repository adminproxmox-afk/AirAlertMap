param([string]$ZipPath)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'release-common.ps1')
$version=Get-AirAlertReleaseVersion
if (-not $ZipPath) { $ZipPath=Join-Path $script:ReleaseRoot "dist\AirAlertMap-$version-win32.zip" }
$ZipPath=[IO.Path]::GetFullPath($ZipPath)
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.Drawing
$expected=@(Get-AirAlertRuntimeFiles) + @('release-manifest.json')
$archive=[IO.Compression.ZipFile]::OpenRead($ZipPath)
try {
    $entryNames=@($archive.Entries | Where-Object { $_.Name } | ForEach-Object { $_.FullName.Replace('\','/') })
    if ($entryNames.Count -ne $expected.Count -or @($entryNames | Select-Object -Unique).Count -ne $entryNames.Count -or
        @(Compare-Object ($expected | Sort-Object) ($entryNames | Sort-Object)).Count) {
        throw 'ZIP content differs from the static runtime allowlist (missing, unexpected, or duplicate files).'
    }
} finally { $archive.Dispose() }
$checksum=(Get-FileHash -LiteralPath $ZipPath -Algorithm SHA256).Hash.ToLowerInvariant()
$sidecar=[IO.File]::ReadAllText($ZipPath+'.sha256').Trim()
if ($sidecar -ne ($checksum+'  '+[IO.Path]::GetFileName($ZipPath))) { throw 'ZIP SHA-256 sidecar does not match.' }
$sandbox=[IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetTempPath()) ('AirAlertMap smoke with spaces '+[Guid]::NewGuid().ToString('N'))))
try {
    Expand-Archive -LiteralPath $ZipPath -DestinationPath $sandbox
    $manifest=Get-Content -LiteralPath (Join-Path $sandbox 'release-manifest.json') -Raw | ConvertFrom-Json
    if ($manifest.version -ne $version -or $manifest.product -ne 'AirAlertMap' -or $manifest.platform -ne 'win32' -or
        $manifest.architecture -ne 'x86' -or $manifest.schemaVersion -ne 1) { throw 'Invalid release manifest identity.' }
    $fileNames=@($manifest.files | ForEach-Object { $_.path })
    if ($fileNames.Count -ne ($expected.Count-1) -or @(Compare-Object (Get-AirAlertRuntimeFiles | Sort-Object) ($fileNames | Sort-Object)).Count) {
        throw 'Manifest file list differs from the static runtime allowlist.'
    }
    foreach ($entry in $manifest.files) {
        $file=Get-Item -LiteralPath (Join-Path $sandbox $entry.path)
        if ($file.Length -ne $entry.size -or (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) {
            throw "Release file integrity mismatch: $($entry.path)"
        }
    }
    # Independently follow the page's actual asset references. This catches a
    # newly added script/style/icon even if the curated list was not updated.
    $indexPath=Join-Path $sandbox 'web\index.html'
    $html=[IO.File]::ReadAllText($indexPath)
    $assetPattern='(?is)<(?:script|link)\b[^>]*?\s(?:src|href)\s*=\s*["'']([^"'']+)["''][^>]*>'
    $packagePrefix=$sandbox.TrimEnd('\')+'\'
    $assetCount=0
    foreach ($match in [regex]::Matches($html,$assetPattern)) {
        $reference=[Net.WebUtility]::HtmlDecode($match.Groups[1].Value)
        if ($reference -match '^(?:[a-z][a-z0-9+.-]*:|//|#)') { continue }
        $uri=[Uri]::new([Uri]$indexPath,$reference)
        $assetPath=[IO.Path]::GetFullPath($uri.LocalPath)
        if (-not $uri.IsFile -or -not $assetPath.StartsWith($packagePrefix,[StringComparison]::OrdinalIgnoreCase)) {
            throw "HTML asset escapes portable folder: $reference"
        }
        $assetRelative=$assetPath.Substring($packagePrefix.Length).Replace('\','/')
        if ($entryNames -notcontains $assetRelative -or $fileNames -notcontains $assetRelative -or
            -not (Test-Path -LiteralPath $assetPath -PathType Leaf)) {
            throw "Local HTML asset is missing from ZIP/manifest: $reference"
        }
        $assetCount++
    }
    if ($assetCount -eq 0) { throw 'No local HTML asset references found; check the release page.' }
    if ([IO.File]::ReadAllText((Join-Path $sandbox 'release-manifest.json')) -ne
        [IO.File]::ReadAllText(($ZipPath -replace '\.zip$','.manifest.json'))) { throw 'External and archived manifests differ.' }
    Assert-AirAlertNativeResources (Join-Path $sandbox 'AirAlertMap.exe') $version
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $sandbox 'data') -Filter '*.json' -File) {
        $content=[IO.File]::ReadAllText($file.FullName)
        if ($content -match '(?i)"(?:api[_-]?key|access[_-]?token|authorization|password|credential)"\s*:\s*"[^"\s]+"') {
            throw "Credential-shaped data in release metadata: $($file.Name)"
        }
    }
    $icon=[Drawing.Icon]::ExtractAssociatedIcon((Join-Path $sandbox 'AirAlertMap.exe'))
    if (-not $icon) { throw 'Windows could not extract the application icon.' }
    $icon.Dispose()
    Write-Host "Release smoke passed: $($entryNames.Count) allowlisted files, $assetCount HTML asset references; hashes, x86 PE, version $version and MAINICON; extraction path contains spaces."
    Write-Host 'No application process launched; no key vault or user-data directory was read.'
} finally {
    $tempRoot=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
    if ($sandbox.StartsWith($tempRoot,[StringComparison]::OrdinalIgnoreCase) -and
        [IO.Path]::GetFileName($sandbox) -match '^AirAlertMap smoke with spaces [a-f0-9]{32}$') {
        if (Test-Path -LiteralPath $sandbox) { Remove-Item -LiteralPath $sandbox -Recurse -Force }
    } else { throw "Refusing to remove unexpected smoke path: $sandbox" }
}
