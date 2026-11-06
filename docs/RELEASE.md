# Portable Windows release

The version lives in `resources/version.h`. `APP_VERSION` supplies the UI and
archive name, and `APP_VERSION_COMMA` / `APP_FILE_VERSION` supply Windows PE
metadata. When changing the version, update these adjacent definitions together.
The current release is **0.4.1**, Win32 x86, with statically linked VCL/RTL.

From the project root in PowerShell:

```powershell
.\tools\build-release.ps1
# Repackage a binary already built with the current version/icon resources:
.\tools\build-release.ps1 -SkipBuild
# Verify an existing release independently, without launching the application:
.\tools\test-release.ps1
```

`-BdsRoot` selects a C++Builder installation. `-BuildDirectory` selects the
directory containing a previously built EXE (for example an IDE build).
`-OutputDirectory` redirects all three artifacts for an isolated CI or packaging test.
An optional `-Version 0.4.1` is an assertion and must equal the resource header;
it cannot relabel an older binary. `-SkipBuild` still checks the executable's
Win32 machine type, native version information, product name and MAINICON.

Outputs:

- `dist/AirAlertMap-0.4.1-win32.zip`
- `dist/AirAlertMap-0.4.1-win32.zip.sha256`
- `dist/AirAlertMap-0.4.1-win32.manifest.json` (also inside the ZIP as `release-manifest.json`)

The staging and extraction folders are unique paths with spaces under the
Windows temporary directory. Only these validated, exact paths are removed.
Existing build output, the running application and user data are untouched.

The ZIP has the executable at its root with `start.cmd`, launch instructions,
the WebView2 x86 loader and its license, the static `web` assets,
`analytics/analyze.py`, and the four static data/attribution files listed in
`tools/release-common.ps1`. No recursive copy of `build` occurs. Tests, object
files, debug symbols, development tools, keys, preferences, observation history,
runtime catalog cache, diagnostics and WebView2 profiles are not shipped.
The runtime checker prints Microsoft and Python download URLs; it never
downloads, installs or opens a browser. Python is optional for the map.

The smoke check rejects extra/missing/duplicate ZIP entries, checks both copies
of the manifest, SHA-256 hashes and sizes, independently follows local script
and link references in index.html to ensure they are in the ZIP and manifest,
then extracts to an isolated path
with spaces and verifies the x86 executable's version and icon resources.
It checks static JSON metadata for populated credential fields. It never reads
the Windows credential store, launches AirAlertMap, or reads LocalAppData.

## Resource build and project wiring

The original icon is in `resources/AirAlertMap.svg` and the matching multi-size
ICO is checked in. Regenerate the ICO with `tools/generate-icon.ps1` (Windows
System.Drawing, no download or art service). The resource file is ASCII and
contains `MAINICON` and English VERSIONINFO. Compile it with:

```powershell
.\tools\prepare-resources.ps1 -BdsRoot 'C:\Program Files (x86)\Embarcadero\Studio\37.0'
```

This invokes `brcc32 -32 -fo<absolute-build-path> AirAlertMap.rc` from the
resources directory, producing `build/AirAlertMap.res`. The command-line
`build.ps1` must invoke this helper before the application compilation and
append `AirAlertMap.res` to the final bcc32 application inputs. Tests do not
need the application resource.

For the IDE project, set `Icon_MainIcon` to `resources\AirAlertMap.ico` and set
`SkipResGeneration=true` and `VerInfo_IncludeVerInfo=false`. Add a target
`PrepareAirAlertResources` with `BeforeTargets="_ResolveOutput"` that invokes
the same helper, passing the IDE output resource path:
`-Destination "$(MSBuildProjectDirectory)\$(BRCC_OutputDir)AirAlertMap.res"`.
The installed CodeGear targets initialize BRCC_OutputDir from
IntermediateOutputDir, detect this existing project resource, and link it.
Do not also add a ResourceCompile item: it would collide with the standard
project-resource filename. Add RC/ICO as `None` and version.h as `HppCompile`
items for IDE visibility if desired.
Avoid adding a second MAINICON resource in the native entry point. The UI can
include `../resources/version.h` and read `APP_VERSION` or `APP_VERSION_W`.

The native resource check is a read-only LoadLibraryEx/FindResource inspection;
it does not execute the application. For a manual check, open the EXE's Windows
Properties > Details and check File version 0.4.1.0 and Product version 0.4.1.
Check the icon in Explorer at both small and large sizes. On a clean Windows
account, extract the ZIP, run start.cmd, set a key, verify a live update and
history reporting (with and without Python), then copy the portable folder to
another path and confirm the UI loads. This interactive check is separate
from the automated smoke check and must not interrupt an existing app session.
