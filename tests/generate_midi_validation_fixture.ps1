param(
    [string]$OutputPath = (Join-Path $PSScriptRoot '..\Audio\second_validation.mid')
)

$ErrorActionPreference = 'Stop'

# Standard MIDI File type 0, 480 ticks/quarter, 120 BPM. Two four-second
# notes produce an eight-second timeline suitable for playback and seek tests.
[byte[]]$bytes = @(
    0x4D,0x54,0x68,0x64, 0x00,0x00,0x00,0x06,
    0x00,0x00, 0x00,0x01, 0x01,0xE0,
    0x4D,0x54,0x72,0x6B, 0x00,0x00,0x00,0x20,
    0x00,0xFF,0x51,0x03,0x07,0xA1,0x20,
    0x00,0xC0,0x00,
    0x00,0x90,0x3C,0x64,
    0x9E,0x00,0x80,0x3C,0x40,
    0x00,0x90,0x43,0x64,
    0x9E,0x00,0x80,0x43,0x40,
    0x00,0xFF,0x2F,0x00
)

$fullPath = [IO.Path]::GetFullPath($OutputPath)
$audioDirectory = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\Audio'))
if (-not $fullPath.StartsWith(
        $audioDirectory + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to write the MIDI fixture outside $audioDirectory."
}
[IO.File]::WriteAllBytes($fullPath, $bytes)

[pscustomobject]@{
    Path = $fullPath
    Bytes = $bytes.Length
    Sha256 = (Get-FileHash -LiteralPath $fullPath -Algorithm SHA256).Hash
}
