# Packs the imported game data (game\userdata\) into a STORE-mode openbeach-data.zip for OpenBeach on Android
# (docs/build/releases.md). Uncompressed zip takes seconds for ~1 GB because PNGs and MP3s are already compressed.
[CmdletBinding()]
param(
    [string]$Output = ''
)

$ErrorActionPreference = 'Stop'
$gameDir = Split-Path -Parent $PSScriptRoot
$userdata = Join-Path $gameDir 'userdata'
$packJson = Join-Path $userdata 'packs\base\pack.json'

if (-not (Test-Path -LiteralPath $packJson)) {
    Write-Host 'No imported game data found in game\userdata\packs\base\pack.json.' -ForegroundColor Red
    Write-Host 'Run Import.cmd first to convert your own copy of Beach Life, then run ExportForAndroid.cmd again.'
    exit 1
}

if (-not $Output) {
    $Output = Join-Path (Split-Path -Parent $gameDir) 'openbeach-data.zip'
}
$temp = "$Output.tmp"
if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Force }

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$roots = @('packs\base', 'remaster', 'SaveGames')
$topFiles = @('BeachLife.ini', 'openbeach.ini')
$entries = [System.Collections.Generic.List[object]]::new()

foreach ($rel in $roots) {
    $full = Join-Path $userdata $rel
    if (-not (Test-Path -LiteralPath $full)) { continue }
    Get-ChildItem -LiteralPath $full -Recurse -File | Sort-Object FullName | ForEach-Object {
        $entryName = $_.FullName.Substring($userdata.Length + 1).Replace('\', '/')
        $entries.Add([pscustomobject]@{ Source = $_.FullName; Entry = $entryName })
    }
}
foreach ($name in $topFiles) {
    $full = Join-Path $userdata $name
    if (Test-Path -LiteralPath $full) {
        $entries.Add([pscustomobject]@{ Source = $full; Entry = $name })
    }
}

Write-Host "Packing $($entries.Count) files from $userdata into $Output ..."
$zip = [System.IO.Compression.ZipFile]::Open($temp, [System.IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($item in $entries) {
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $zip, $item.Source, $item.Entry, [System.IO.Compression.CompressionLevel]::NoCompression) | Out-Null
    }
} finally {
    $zip.Dispose()
}

if (Test-Path -LiteralPath $Output) { Remove-Item -LiteralPath $Output -Force }
Move-Item -LiteralPath $temp -Destination $Output

$sizeMb = [math]::Round((Get-Item -LiteralPath $Output).Length / 1MB, 1)
Write-Host ""
Write-Host "Done: $Output ($sizeMb MB)" -ForegroundColor Green
Write-Host "Copy this file and the OpenBeach .apk to your Android device (e.g. Downloads), install the .apk,"
Write-Host "tap 'Import data package (.zip)' and choose openbeach-data.zip. Afterwards you can delete the .zip."
