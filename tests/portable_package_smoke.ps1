param(
    [string]$PackageDirectory = (Join-Path $PSScriptRoot '..\portable_app_audiocommander_v4'),
    [string]$BuiltExecutable = (Join-Path $PSScriptRoot '..\audiocommander_v4.exe')
)

$ErrorActionPreference = 'Stop'

$package = (Resolve-Path -LiteralPath $PackageDirectory).Path
$built = (Resolve-Path -LiteralPath $BuiltExecutable).Path
$executableName = Split-Path -Leaf $built
$isV93 = $executableName -eq 'audiocommander_v93.exe'
$isTrackerRelease = $executableName -match '^audiocommander_v(?:8|9|91|92|93)\.exe$'
$isSkinRelease = $executableName -in @('audiocommander_v91.exe', 'audiocommander_v92.exe', 'audiocommander_v93.exe')
$expectedFiles = @(
    $executableName,
    'avcodec-63.dll',
    'avformat-63.dll',
    'avutil-61.dll',
    'FFmpeg-LICENSE.txt',
    'PACKAGE_SHA256.txt',
    'PORTABLE_CONTENTS.txt',
    'README.txt',
    'source\build_ffmpeg_minimal.ps1',
    'source\ffmpeg_minimal_config.txt',
    'swresample-7.dll',
    'THIRD_PARTY_NOTICES.md'
)
if ($isV93) {
    $expectedFiles += @(
        'libopenmpt.dll',
        'libopenmpt-LICENSE.txt',
        'mpg123-AUTHORS.txt',
        'mpg123-LICENSE.txt',
        'ogg-LICENSE.txt',
        'openmpt-mpg123.dll',
        'openmpt-ogg.dll',
        'openmpt-vorbis.dll',
        'openmpt-zlib.dll',
        'vorbis-LICENSE.txt',
        'zlib-LICENSE.txt'
    )
} elseif ($isTrackerRelease) {
    $expectedFiles += @(
        'libopenmpt-LICENSE.txt',
        'source\build_libopenmpt_small.txt',
        'source\libopenmpt-0.8.7+release.msvc.zip'
    )
}
if (-not $isV93) {
    $expectedFiles += 'source\FFmpeg-c23123630e-source.zip'
}
if ($isSkinRelease) {
    $expectedFiles += @(
        'skins\Acryl.skn',
        'skins\Air.skn',
        'skins\MetroUI.skn',
        'skins\Office2003.skn',
        'skins\Office2007 Black.skn',
        'skins\XPLuna.skn',
        'skins\XPSilver.skn',
        'skins\Zest.skn'
    )
}

$actualFiles = @(
    Get-ChildItem -LiteralPath $package -File -Recurse |
        ForEach-Object { $_.FullName.Substring($package.Length + 1) } |
        Sort-Object
)
$expectedSorted = @($expectedFiles | Sort-Object)
if (Compare-Object -ReferenceObject $expectedSorted -DifferenceObject $actualFiles) {
    throw 'Portable package contents differ from the declared release set.'
}
if (Test-Path -LiteralPath (Join-Path $package 'audiocommander.ini')) {
    throw 'A pre-accepted INI must not be shipped.'
}

$manifestPath = Join-Path $package 'PACKAGE_SHA256.txt'
$manifestLines = Get-Content -LiteralPath $manifestPath
$verified = 0
foreach ($line in $manifestLines) {
    if ($line -notmatch '^([0-9A-F]{64})  (.+)$') {
        continue
    }
    $expectedHash = $Matches[1]
    $relativePath = $Matches[2]
    $filePath = Join-Path $package $relativePath
    if (-not (Test-Path -LiteralPath $filePath -PathType Leaf)) {
        throw "Manifest file is missing: $relativePath"
    }
    $actualHash = (Get-FileHash -LiteralPath $filePath -Algorithm SHA256).Hash
    if ($actualHash -ne $expectedHash) {
        throw "SHA-256 mismatch: $relativePath"
    }
    $verified++
}
$expectedManifestEntries = if ($isV93) { 30 } elseif ($isSkinRelease) { 23 } elseif ($isTrackerRelease) { 15 } else { 12 }
if ($verified -ne $expectedManifestEntries) {
    throw "Expected $expectedManifestEntries manifest entries; verified $verified."
}

$builtHash = (Get-FileHash -LiteralPath $built -Algorithm SHA256).Hash
$packageExeHash = (Get-FileHash -LiteralPath (
    Join-Path $package $executableName) -Algorithm SHA256).Hash
if ($builtHash -ne $packageExeHash) {
    throw 'Packaged executable does not match the current release build.'
}

[pscustomobject]@{
    Files = $actualFiles.Count
    ManifestEntriesVerified = $verified
    PreAcceptedIniPresent = $false
    ExecutableSha256 = $packageExeHash
}
