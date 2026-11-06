param([string]$Destination = (Join-Path $PSScriptRoot '..\build\geodata-research'))
$ErrorActionPreference='Stop'
$Destination=[IO.Path]::GetFullPath($Destination)
New-Item -ItemType Directory -Path $Destination -Force | Out-Null
$archive=Join-Path $Destination 'ukr_admin_boundaries.geojson.zip'
$source='https://data.humdata.org/dataset/d23f529f-31e4-4021-a65b-13987e5cfb42/resource/681beb86-391b-4a08-8140-ca52e80fcdce/download/ukr_admin_boundaries.geojson.zip'
$expected='B8604BC983E1E54A6F7C10186A5CF8474CB8ED49EB038B4BEF8091D92C286790'
if (-not (Test-Path -LiteralPath $archive)) { Invoke-WebRequest -Uri $source -OutFile $archive }
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expected) { throw 'Geometry source changed; review metadata and mapping before updating.' }
Expand-Archive -LiteralPath $archive -DestinationPath $Destination -Force
$raw=Get-ChildItem -LiteralPath $Destination -Filter 'ukr_admin1.geojson' -File -Recurse | Select-Object -First 1
if (-not $raw) { throw 'Source archive layout changed.' }
$inputDir=$raw.DirectoryName
$outputDir=Join-Path $Destination 'simplified'
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
& npx.cmd --yes mapshaper@0.7.101 -i (Join-Path $inputDir 'ukr_admin1.geojson') (Join-Path $inputDir 'ukr_admin2.geojson') (Join-Path $inputDir 'ukr_admin3.geojson') combine-files -simplify '3%' keep-shapes -o 'target=*' format=geojson precision=0.00001 $outputDir
if ($LASTEXITCODE -ne 0) { throw 'Geometry simplification failed.' }
& node (Join-Path $PSScriptRoot 'generate-details.js') $outputDir
if ($LASTEXITCODE -ne 0) { throw 'Geometry generation failed.' }
