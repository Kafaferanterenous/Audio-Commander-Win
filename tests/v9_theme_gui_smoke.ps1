param(
    [string]$Executable = (Join-Path $PSScriptRoot '..\build-v9-release\audiocommander_v9.exe'),
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\build-v9-nmake\theme-gui-v9')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class AudioCommanderThemeNative
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Rect { public int Left, Top, Right, Bottom; }

    [DllImport("user32.dll")]
    public static extern IntPtr GetDlgItem(IntPtr window, int controlId);

    [DllImport("user32.dll")]
    public static extern IntPtr GetWindowLongPtrW(IntPtr window, int index);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr window, out Rect rectangle);

    [DllImport("user32.dll")]
    public static extern bool SetWindowPos(
        IntPtr window, IntPtr insertAfter, int x, int y, int width, int height, uint flags);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr window);

    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr window, IntPtr deviceContext, uint flags);

    [DllImport("user32.dll")]
    public static extern bool PostMessageW(
        IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern IntPtr GetDC(IntPtr window);

    [DllImport("user32.dll")]
    public static extern int ReleaseDC(IntPtr window, IntPtr deviceContext);

    [DllImport("gdi32.dll")]
    public static extern uint GetPixel(IntPtr deviceContext, int x, int y);
}
'@

function Save-WindowImage {
    param([IntPtr]$Window, [string]$Path)

    [void][AudioCommanderThemeNative]::SetForegroundWindow($Window)
    Start-Sleep -Milliseconds 300
    $rectangle = New-Object AudioCommanderThemeNative+Rect
    if (-not [AudioCommanderThemeNative]::GetWindowRect(
            $Window, [ref]$rectangle)) {
        throw 'GetWindowRect failed.'
    }
    $width = $rectangle.Right - $rectangle.Left
    $height = $rectangle.Bottom - $rectangle.Top
    $bitmap = [Drawing.Bitmap]::new($width, $height)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        $deviceContext = $graphics.GetHdc()
        try {
            if (-not [AudioCommanderThemeNative]::PrintWindow(
                    $Window, $deviceContext, 0x00000002)) {
                throw 'PrintWindow failed.'
            }
        }
        finally {
            $graphics.ReleaseHdc($deviceContext)
        }
        $bitmap.Save($Path, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Get-ControlPixel {
    param([IntPtr]$Control, [int]$X, [int]$Y)

    $dc = [AudioCommanderThemeNative]::GetDC($Control)
    if ($dc -eq [IntPtr]::Zero) { throw 'GetDC failed.' }
    try {
        return [AudioCommanderThemeNative]::GetPixel($dc, $X, $Y)
    }
    finally {
        [void][AudioCommanderThemeNative]::ReleaseDC($Control, $dc)
    }
}

$source = (Resolve-Path -LiteralPath $Executable).Path
if (Test-Path -LiteralPath $OutputDirectory) {
    throw "Refusing to overwrite existing theme fixture: $OutputDirectory"
}
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$fixture = (Resolve-Path -LiteralPath $OutputDirectory).Path
Copy-Item -LiteralPath $source -Destination $fixture
$sourceDirectory = Split-Path -Parent $source
$libraries = @(Get-ChildItem -LiteralPath $sourceDirectory -Filter '*.dll' |
    Where-Object { $_.Name -match '^(avcodec|avformat|avutil|swresample)-\d+\.dll$' })
if ($libraries.Count -ne 4) {
    throw "Expected four FFmpeg runtime DLLs; found $($libraries.Count)."
}
foreach ($library in $libraries) {
    Copy-Item -LiteralPath $library.FullName -Destination $fixture
}

$fixtureExecutable = Join-Path $fixture (Split-Path -Leaf $source)
$ini = Join-Path $fixture 'audiocommander.ini'
$results = foreach ($case in @(
    [pscustomobject]@{Name='windows-native'; Theme=5; ExpectedButtonType=0},
    [pscustomobject]@{Name='office-2003'; Theme=6; ExpectedButtonType=11}
)) {
    [IO.File]::WriteAllLines(
        $ini,
        @(
            '[Legal]', 'Accepted=1',
            '[Appearance]', 'Version=4', "Theme=$($case.Theme)",
            'Language=0', 'InterfaceSize=100', 'Opacity=100',
            '[Playback]', 'Sequential=0', 'Volume=0',
            '[Folders]', 'Remember=0',
            '[Window]', 'RememberPosition=0'
        ),
        [Text.Encoding]::Unicode)
    $process = Start-Process -FilePath $fixtureExecutable -WorkingDirectory $fixture -PassThru
    $window = [IntPtr]::Zero
    try {
        $deadline = [DateTime]::UtcNow.AddSeconds(12)
        do {
            Start-Sleep -Milliseconds 50
            $process.Refresh()
            $window = [IntPtr]$process.MainWindowHandle
        } while ($window -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $deadline)
        if ($window -eq [IntPtr]::Zero) {
            throw "Main window not found for $($case.Name)."
        }
        [void][AudioCommanderThemeNative]::SetWindowPos(
            $window, [IntPtr]::Zero, 8, 8, 1382, 864, 0x0004)
        Start-Sleep -Milliseconds 500
        $settingsButton = [AudioCommanderThemeNative]::GetDlgItem($window, 10)
        $deleteButton = [AudioCommanderThemeNative]::GetDlgItem($window, 22)
        $summary = [AudioCommanderThemeNative]::GetDlgItem($window, 105)
        if ($settingsButton -eq [IntPtr]::Zero -or
            $deleteButton -eq [IntPtr]::Zero -or
            $summary -eq [IntPtr]::Zero) {
            throw "Required controls not found for $($case.Name)."
        }
        $settingsType = ([AudioCommanderThemeNative]::GetWindowLongPtrW(
            $settingsButton, -16).ToInt64() -band 0xF)
        $deleteType = ([AudioCommanderThemeNative]::GetWindowLongPtrW(
            $deleteButton, -16).ToInt64() -band 0xF)
        if ($settingsType -ne $case.ExpectedButtonType -or
            $deleteType -ne $case.ExpectedButtonType) {
            throw "Unexpected button styles for $($case.Name): settings=$settingsType delete=$deleteType"
        }
        $summaryPixel = Get-ControlPixel -Control $summary -X 3 -Y 3
        if ($case.Theme -eq 6) {
            $expectedSummary = 245 + (233 -shl 8) + (190 -shl 16)
            if ($summaryPixel -ne $expectedSummary) {
                throw ('Office summary colour mismatch: expected {0:X6}, actual {1:X6}' -f
                       $expectedSummary, $summaryPixel)
            }
        }
        $screenshot = Join-Path $fixture ($case.Name + '.png')
        Save-WindowImage -Window $window -Path $screenshot
        [pscustomobject]@{
            Theme = $case.Name
            SettingsButtonType = $settingsType
            DeleteButtonType = $deleteType
            SummaryPixel = ('{0:X6}' -f $summaryPixel)
            Screenshot = $screenshot
        }
    }
    finally {
        if ($window -ne [IntPtr]::Zero) {
            [void][AudioCommanderThemeNative]::PostMessageW(
                $window, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
        }
        if (-not $process.WaitForExit(8000)) {
            throw "AudioCommander did not exit cleanly for $($case.Name)."
        }
    }
}

$results
