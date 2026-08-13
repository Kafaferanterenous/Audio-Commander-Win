param(
    [string]$Executable = (Join-Path $PSScriptRoot '..\build-v5-nmake\audiocommander_v5.exe'),
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\build-v5-nmake\live-gui-layouts-v5'),
    [switch]$ExpectCompactBoldSummary
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class AudioCommanderLayoutNative
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Rect { public int Left, Top, Right, Bottom; }

    [StructLayout(LayoutKind.Sequential)]
    public struct Size { public int Width, Height; }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct LogFont
    {
        public int Height, Width, Escapement, Orientation, Weight;
        public byte Italic, Underline, StrikeOut, CharacterSet;
        public byte OutPrecision, ClipPrecision, Quality, PitchAndFamily;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)]
        public string FaceName;
    }

    [DllImport("user32.dll")]
    public static extern IntPtr GetDlgItem(IntPtr window, int controlId);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowTextW(IntPtr window, StringBuilder text, int capacity);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr window, out Rect rectangle);

    [DllImport("user32.dll")]
    public static extern IntPtr SendMessageW(
        IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr window, IntPtr deviceContext, uint flags);

    [DllImport("user32.dll")]
    public static extern IntPtr GetDC(IntPtr window);

    [DllImport("user32.dll")]
    public static extern int ReleaseDC(IntPtr window, IntPtr deviceContext);

    [DllImport("gdi32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetObjectW(IntPtr handle, int size, out LogFont value);

    [DllImport("gdi32.dll")]
    public static extern IntPtr SelectObject(IntPtr deviceContext, IntPtr value);

    [DllImport("gdi32.dll", CharSet = CharSet.Unicode)]
    public static extern bool GetTextExtentPoint32W(
        IntPtr deviceContext, string text, int length, out Size size);

    [DllImport("user32.dll")]
    public static extern bool SetWindowPos(
        IntPtr window, IntPtr insertAfter, int x, int y, int width, int height, uint flags);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr window);

    [DllImport("user32.dll")]
    public static extern bool PostMessageW(
        IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
}
'@

function Get-ControlText {
    param([IntPtr]$Control)

    $text = [Text.StringBuilder]::new(512)
    [void][AudioCommanderLayoutNative]::GetWindowTextW(
        $Control, $text, $text.Capacity)
    return $text.ToString()
}

function Get-ControlHeight {
    param([IntPtr]$Control)

    $rectangle = New-Object AudioCommanderLayoutNative+Rect
    if (-not [AudioCommanderLayoutNative]::GetWindowRect(
            $Control, [ref]$rectangle)) {
        throw 'GetWindowRect failed for a summary control.'
    }
    return $rectangle.Bottom - $rectangle.Top
}

function Get-ControlFontWeight {
    param([IntPtr]$Control)

    $font = [AudioCommanderLayoutNative]::SendMessageW(
        $Control, 0x0031, [IntPtr]::Zero, [IntPtr]::Zero)
    if ($font -eq [IntPtr]::Zero) { throw 'Summary control has no font.' }
    $description = New-Object AudioCommanderLayoutNative+LogFont
    $size = [Runtime.InteropServices.Marshal]::SizeOf($description)
    if ([AudioCommanderLayoutNative]::GetObjectW(
            $font, $size, [ref]$description) -ne $size) {
        throw 'Could not inspect the summary font.'
    }
    return $description.Weight
}

function Get-ControlTextWidth {
    param([IntPtr]$Control, [string]$Text)

    $font = [AudioCommanderLayoutNative]::SendMessageW(
        $Control, 0x0031, [IntPtr]::Zero, [IntPtr]::Zero)
    $dc = [AudioCommanderLayoutNative]::GetDC($Control)
    if ($dc -eq [IntPtr]::Zero) { throw 'Could not get a summary device context.' }
    $oldFont = [AudioCommanderLayoutNative]::SelectObject($dc, $font)
    try {
        $size = New-Object AudioCommanderLayoutNative+Size
        if (-not [AudioCommanderLayoutNative]::GetTextExtentPoint32W(
                $dc, $Text, $Text.Length, [ref]$size)) {
            throw 'Could not measure summary text.'
        }
        return $size.Width
    }
    finally {
        if ($oldFont -ne [IntPtr]::Zero) {
            [void][AudioCommanderLayoutNative]::SelectObject($dc, $oldFont)
        }
        [void][AudioCommanderLayoutNative]::ReleaseDC($Control, $dc)
    }
}

function Save-WindowImage {
    param([IntPtr]$Window, [string]$Path)

    [void][AudioCommanderLayoutNative]::SetForegroundWindow($Window)
    Start-Sleep -Milliseconds 200
    $rectangle = New-Object AudioCommanderLayoutNative+Rect
    if (-not [AudioCommanderLayoutNative]::GetWindowRect($Window, [ref]$rectangle)) {
        throw 'GetWindowRect failed.'
    }
    $width = $rectangle.Right - $rectangle.Left
    $height = $rectangle.Bottom - $rectangle.Top
    $bitmap = [Drawing.Bitmap]::new($width, $height)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        try {
            $graphics.CopyFromScreen(
                $rectangle.Left, $rectangle.Top, 0, 0,
                [Drawing.Size]::new($width, $height))
        }
        catch [ComponentModel.Win32Exception] {
            $deviceContext = $graphics.GetHdc()
            try {
                if (-not [AudioCommanderLayoutNative]::PrintWindow(
                        $Window, $deviceContext, 0x00000002)) {
                    throw 'Both CopyFromScreen and PrintWindow failed.'
                }
            }
            finally {
                $graphics.ReleaseHdc($deviceContext)
            }
        }
        $bitmap.Save($Path, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$sourceDirectory = Split-Path -Parent $resolvedExecutable
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$resolvedOutput = (Resolve-Path -LiteralPath $OutputDirectory).Path
$temporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$fixture = Join-Path $temporaryRoot (
    'AudioCommander-layout-' + [Guid]::NewGuid().ToString('N'))
$fixture = [IO.Path]::GetFullPath($fixture)
if (-not $fixture.StartsWith($temporaryRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Refusing to create a layout fixture outside the temporary directory.'
}

$cases = @(
    [pscustomobject]@{ Name = 'scale-75-english'; Scale = 75; Language = 0;
        Width = 1100; Height = 720; Browse = 'Browse...' },
    [pscustomobject]@{ Name = 'scale-100-english'; Scale = 100; Language = 0;
        Width = 1382; Height = 864; Browse = 'Browse...' },
    [pscustomobject]@{ Name = 'scale-200-english'; Scale = 200; Language = 0;
        Width = 1800; Height = 1000; Browse = 'Browse...' },
    [pscustomobject]@{ Name = 'chinese-light'; Scale = 100; Language = 1;
        Width = 1382; Height = 864; Browse = ([string][char]0x6D4F + [char]0x89C8 + '...') },
    [pscustomobject]@{ Name = 'polish-light'; Scale = 100; Language = 3;
        Width = 1382; Height = 864;
        Browse = ('Przegl' + [char]0x0105 + 'daj...') }
)

$app = $null
try {
    New-Item -ItemType Directory -Path $fixture | Out-Null
    Copy-Item -LiteralPath $resolvedExecutable -Destination $fixture
    $runtimeLibraries = @(Get-ChildItem -LiteralPath $sourceDirectory -Filter '*.dll' |
        Where-Object { $_.Name -match '^(avcodec|avformat|avutil|swresample)-\d+\.dll$' })
    if ($runtimeLibraries.Count -ne 4) {
        throw "Expected four FFmpeg runtime libraries; found $($runtimeLibraries.Count)."
    }
    foreach ($library in $runtimeLibraries) {
        Copy-Item -LiteralPath $library.FullName -Destination $fixture
    }
    $fixtureExecutable = Join-Path $fixture (Split-Path -Leaf $resolvedExecutable)
    $ini = Join-Path $fixture 'audiocommander.ini'

    $results = foreach ($case in $cases) {
        [IO.File]::WriteAllLines(
            $ini,
            @(
                '[Legal]', 'Accepted=1',
                '[Appearance]', 'Opacity=100', 'Theme=1',
                "Language=$($case.Language)", "InterfaceSize=$($case.Scale)",
                '[Playback]', 'Sequential=0', 'Volume=0',
                '[Window]', 'RememberPosition=0'
            ),
            [Text.Encoding]::Unicode)
        $main = [IntPtr]::Zero
        $app = Start-Process -FilePath $fixtureExecutable -WorkingDirectory $fixture -PassThru
        try {
            $deadline = [DateTime]::UtcNow.AddSeconds(8)
            do {
                Start-Sleep -Milliseconds 50
                $app.Refresh()
                $main = [IntPtr]$app.MainWindowHandle
            } while ($main -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $deadline)
            if ($main -eq [IntPtr]::Zero) {
                throw "Main window not found for $($case.Name)."
            }
            if (-not [AudioCommanderLayoutNative]::SetWindowPos(
                    $main, [IntPtr]::Zero, 8, 8, $case.Width, $case.Height, 0x0004)) {
                throw "SetWindowPos failed for $($case.Name)."
            }
            Start-Sleep -Milliseconds 250
            $leftBrowse = Get-ControlText (
                [AudioCommanderLayoutNative]::GetDlgItem($main, 104))
            $rightBrowse = Get-ControlText (
                [AudioCommanderLayoutNative]::GetDlgItem($main, 204))
            if ($leftBrowse -ne $case.Browse -or $rightBrowse -ne $case.Browse) {
                throw "$($case.Name) Browse labels were '$leftBrowse' and '$rightBrowse'."
            }
            $summaryResult = 'not requested'
            if ($ExpectCompactBoldSummary) {
                $leftSummary = [AudioCommanderLayoutNative]::GetDlgItem($main, 105)
                $rightSummary = [AudioCommanderLayoutNative]::GetDlgItem($main, 205)
                if ($leftSummary -eq [IntPtr]::Zero -or
                    $rightSummary -eq [IntPtr]::Zero) {
                    throw "$($case.Name) summary controls were not found."
                }
                $oldMinimumHeight = [int][Math]::Round(24 * $case.Scale / 100)
                $leftHeight = Get-ControlHeight $leftSummary
                $rightHeight = Get-ControlHeight $rightSummary
                $heightTooLarge = if ($case.Scale -le 75) {
                    $leftHeight -gt $oldMinimumHeight -or
                    $rightHeight -gt $oldMinimumHeight
                } else {
                    $leftHeight -ge $oldMinimumHeight -or
                    $rightHeight -ge $oldMinimumHeight
                }
                if ($heightTooLarge) {
                    throw "$($case.Name) summary height was not reduced (left=$leftHeight, right=$rightHeight, old minimum=$oldMinimumHeight)."
                }
                $leftWeight = Get-ControlFontWeight $leftSummary
                $rightWeight = Get-ControlFontWeight $rightSummary
                if ($leftWeight -lt 700 -or $rightWeight -lt 700) {
                    throw "$($case.Name) summary font is not bold (left=$leftWeight, right=$rightWeight)."
                }
                $leftSummaryText = Get-ControlText $leftSummary
                $rightSummaryText = Get-ControlText $rightSummary
                $leftRect = New-Object AudioCommanderLayoutNative+Rect
                $rightRect = New-Object AudioCommanderLayoutNative+Rect
                [void][AudioCommanderLayoutNative]::GetWindowRect(
                    $leftSummary, [ref]$leftRect)
                [void][AudioCommanderLayoutNative]::GetWindowRect(
                    $rightSummary, [ref]$rightRect)
                $leftAvailable = $leftRect.Right - $leftRect.Left
                $rightAvailable = $rightRect.Right - $rightRect.Left
                $leftRequired = Get-ControlTextWidth $leftSummary $leftSummaryText
                $rightRequired = Get-ControlTextWidth $rightSummary $rightSummaryText
                if ($leftRequired + 4 -gt $leftAvailable -or
                    $rightRequired + 4 -gt $rightAvailable) {
                    throw "$($case.Name) summary text does not fit (left=$leftRequired/$leftAvailable, right=$rightRequired/$rightAvailable)."
                }
                $summaryResult = "passed ($leftHeight px, weight $leftWeight, text $leftRequired/$leftAvailable px)"
            }
            $imagePath = Join-Path $resolvedOutput ($case.Name + '.png')
            Save-WindowImage -Window $main -Path $imagePath
            [pscustomobject]@{
                Layout = $case.Name
                Scale = $case.Scale
                Language = $case.Language
                BrowseLabels = 'passed'
                CompactBoldSummary = $summaryResult
                Screenshot = $imagePath
            }
        }
        finally {
            if ($app -and -not $app.HasExited) {
                if ($main -ne [IntPtr]::Zero) {
                    [void][AudioCommanderLayoutNative]::PostMessageW(
                        $main, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
                    [void]$app.WaitForExit(3000)
                }
                if (-not $app.HasExited) {
                    Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
                    [void]$app.WaitForExit(3000)
                }
            }
            $app = $null
        }
    }
    $results
}
finally {
    if ($app -and -not $app.HasExited) {
        Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
    }
    if (Test-Path -LiteralPath $fixture) {
        $cleanupPath = [IO.Path]::GetFullPath($fixture)
        if (-not $cleanupPath.StartsWith(
                $temporaryRoot, [StringComparison]::OrdinalIgnoreCase) -or
            -not (Split-Path -Leaf $cleanupPath).StartsWith('AudioCommander-layout-')) {
            throw "Refusing unsafe layout-fixture cleanup path: $cleanupPath"
        }
        Remove-Item -LiteralPath $cleanupPath -Recurse -Force
    }
}
