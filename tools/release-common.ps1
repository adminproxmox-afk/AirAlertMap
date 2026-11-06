$script:ReleaseRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
function Get-AirAlertReleaseVersion {
    $header=[IO.File]::ReadAllText((Join-Path $script:ReleaseRoot 'resources\version.h'))
    $match=[regex]::Match($header,'(?m)^#define APP_VERSION "(\d+\.\d+\.\d+)"\s*$')
    if (-not $match.Success) { throw 'Invalid APP_VERSION in resources/version.h.' }
    $version=$match.Groups[1].Value
    $expectedComma=$version.Replace('.',',')+',0'
    if ($header -notmatch ('(?m)^#define APP_VERSION_W L"'+[regex]::Escape($version)+'"\s*$') -or
        $header -notmatch ('(?m)^#define APP_FILE_VERSION "'+[regex]::Escape($version)+'\.0"\s*$') -or
        $header -notmatch ('(?m)^#define APP_VERSION_COMMA '+[regex]::Escape($expectedComma)+'\s*$')) {
        throw 'Version definitions in resources/version.h disagree.'
    }
    return $version
}
function Get-AirAlertRuntimeFiles {
    # Explicit static allowlist. Never copy the build folder or user-data trees.
    return @('AirAlertMap.exe','WebView2Loader.dll','WebView2-LICENSE.txt',
        'README.md','start.cmd','check-runtime.ps1',
        'web/app.js','web/detail-model.js','web/favicon.svg','web/index.html',
        'web/map-model.js','web/map.css','web/map.js','web/preferences.js','web/regions.js','web/territories.js',
        'analytics/analyze.py','analytics/manage_history.py','data/regions.json','data/NOTICE.md',
        'data/metadata.json','data/territory-mapping.json')
}
function Assert-AirAlertNativeResources([string]$Executable,[string]$Version) {
    $bytes=[IO.File]::ReadAllBytes($Executable)
    if ($bytes.Length -lt 256 -or [BitConverter]::ToUInt16($bytes,0) -ne 0x5a4d) { throw 'Executable is not a Windows PE file.' }
    $pe=[BitConverter]::ToInt32($bytes,0x3c)
    if ($pe -lt 0 -or $pe+26 -gt $bytes.Length -or [BitConverter]::ToUInt32($bytes,$pe) -ne 0x4550 -or
        [BitConverter]::ToUInt16($bytes,$pe+4) -ne 0x14c -or [BitConverter]::ToUInt16($bytes,$pe+24) -ne 0x10b) { throw 'Release requires a Win32 x86 executable.' }
    $info=[Diagnostics.FileVersionInfo]::GetVersionInfo($Executable)
    if ($info.ProductVersion -ne $Version -or $info.FileVersion -ne "$Version.0" -or $info.ProductName -ne 'AirAlertMap') {
        throw "PE version metadata does not match $Version. Rebuild with resources/AirAlertMap.rc."
    }
    if (-not ('AirAlertMap.ReleaseResourceProbe' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace AirAlertMap {
 public static class ReleaseResourceProbe {
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern IntPtr LoadLibraryEx(string name,IntPtr file,uint flags);
  [DllImport("kernel32.dll",EntryPoint="FindResourceW",CharSet=CharSet.Unicode)] public static extern IntPtr FindResource(IntPtr module,string name,IntPtr type);
  [DllImport("kernel32.dll")] public static extern bool FreeLibrary(IntPtr module);
 }
}
'@
    }
    $module=[AirAlertMap.ReleaseResourceProbe]::LoadLibraryEx($Executable,[IntPtr]::Zero,2)
    if ($module -eq [IntPtr]::Zero) { throw 'Cannot inspect executable resources.' }
    try {
        if ([AirAlertMap.ReleaseResourceProbe]::FindResource($module,'MAINICON',[IntPtr]14) -eq [IntPtr]::Zero) { throw 'PE MAINICON resource is missing.' }
    } finally { [void][AirAlertMap.ReleaseResourceProbe]::FreeLibrary($module) }
}
