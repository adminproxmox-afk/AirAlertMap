param(
    [switch]$SkipBuild,
    [string]$Version,
    [string]$BdsRoot = $env:BDS,
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\build'),
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\dist')
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'release-common.ps1')
$releaseVersion=Get-AirAlertReleaseVersion
if ($Version -and $Version -ne $releaseVersion) { throw "Version must match resources/version.h ($releaseVersion)." }
if (-not $SkipBuild) {
    $buildArguments=@{}
    if ($BdsRoot) { $buildArguments.BdsRoot=$BdsRoot }
    & (Join-Path $script:ReleaseRoot 'build.ps1') @buildArguments
}
$executable=[IO.Path]::GetFullPath((Join-Path $BuildDirectory 'AirAlertMap.exe'))
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw "Application missing: $executable" }
Assert-AirAlertNativeResources $executable $releaseVersion
& (Join-Path $PSScriptRoot 'prepare-webview.ps1')
$dist=[IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($dist) | Out-Null
$stage=[IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetTempPath()) ('AirAlertMap release '+[Guid]::NewGuid().ToString('N'))))
[IO.Directory]::CreateDirectory($stage) | Out-Null
try {
    foreach ($relative in Get-AirAlertRuntimeFiles) {
        $source=Join-Path $script:ReleaseRoot $relative
        switch ($relative) {
            'AirAlertMap.exe' { $source=$executable }
            'WebView2Loader.dll' { $source=Join-Path $script:ReleaseRoot 'vendor\webview2\sdk\build\native\x86\WebView2Loader.dll' }
            'WebView2-LICENSE.txt' { $source=Join-Path $script:ReleaseRoot 'vendor\webview2\sdk\LICENSE.txt' }
            'start.cmd' { $source=Join-Path $script:ReleaseRoot 'resources\start.cmd' }
            'check-runtime.ps1' { $source=Join-Path $script:ReleaseRoot 'resources\check-runtime.ps1' }
            'README.md' { $source=Join-Path $script:ReleaseRoot 'resources\portable-README.md' }
        }
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Required runtime source missing: $relative" }
        $target=Join-Path $stage $relative
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
        if ($relative -eq 'README.md') {
            [IO.File]::WriteAllText($target,[IO.File]::ReadAllText($source).Replace('@VERSION@',$releaseVersion),[Text.UTF8Encoding]::new($false))
        } else { Copy-Item -LiteralPath $source -Destination $target }
    }
    $files=@(foreach ($relative in (Get-AirAlertRuntimeFiles | Sort-Object)) {
        $file=Get-Item -LiteralPath (Join-Path $stage $relative)
        [ordered]@{ path=$relative; size=$file.Length; sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
    })
    $manifest=[ordered]@{
        schemaVersion=1; product='AirAlertMap'; version=$releaseVersion; platform='win32'; architecture='x86'
        createdUtc=[DateTime]::UtcNow.ToString('o'); files=$files
    } | ConvertTo-Json -Depth 5
    $manifestPath=Join-Path $stage 'release-manifest.json'
    [IO.File]::WriteAllText($manifestPath,$manifest,[Text.UTF8Encoding]::new($false))
    $zip=Join-Path $dist "AirAlertMap-$releaseVersion-win32.zip"
    # Compress only freshly staged allowlisted runtime files. Existing build
    # artifacts and LocalAppData are never enumerated or copied.
    Compress-Archive -LiteralPath @((Get-ChildItem -LiteralPath $stage).FullName) -DestinationPath $zip -CompressionLevel Optimal -Force
    Copy-Item -LiteralPath $manifestPath -Destination ($zip -replace '\.zip$','.manifest.json') -Force
    $checksum=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
    [IO.File]::WriteAllText(($zip+'.sha256'),($checksum+'  '+[IO.Path]::GetFileName($zip)+"`r`n"),[Text.Encoding]::ASCII)
    & (Join-Path $PSScriptRoot 'test-release.ps1') -ZipPath $zip
    Write-Host "Portable release: $zip"
    Write-Host "SHA-256: $checksum"
} finally {
    # The only recursive removal is the unique directory created by this run.
    $tempRoot=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
    if ($stage.StartsWith($tempRoot,[StringComparison]::OrdinalIgnoreCase) -and
        [IO.Path]::GetFileName($stage) -match '^AirAlertMap release [a-f0-9]{32}$') {
        Remove-Item -LiteralPath $stage -Recurse -Force
    } else { throw "Refusing to remove unexpected staging path: $stage" }
}
