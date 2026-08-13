param(
    [string]$Executable = (Join-Path $PSScriptRoot '..\build-v6-release\audiocommander_v6.exe'),
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\build-v6-release\settings-layouts-v6')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class AudioCommanderV6SettingsNative
{
    public delegate bool EnumWindowProc(IntPtr window, IntPtr parameter);

    [StructLayout(LayoutKind.Sequential)]
    public struct Rect { public int Left, Top, Right, Bottom; }

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowProc callback, IntPtr parameter);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassNameW(IntPtr window, StringBuilder text, int capacity);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindowW(string className, string windowName);

    [DllImport("user32.dll")]
    public static extern IntPtr GetDlgItem(IntPtr window, int controlId);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "SendMessageW")]
    public static extern IntPtr SendMessageTextW(
        IntPtr window, uint message, IntPtr wParam, StringBuilder lParam);

    [DllImport("user32.dll")]
    public static extern IntPtr SendMessageW(
        IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool PostMessageW(
        IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr window, out Rect rectangle);

    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr window, IntPtr deviceContext, uint flags);
}
'@

function Get-WindowByClass {
    param([int]$ProcessId, [string]$Class)

    $windows = [Collections.Generic.List[object]]::new()
    $callback = [AudioCommanderV6SettingsNative+EnumWindowProc]{
        param([IntPtr]$window, [IntPtr]$parameter)
        [uint32]$owner = 0
        [void][AudioCommanderV6SettingsNative]::GetWindowThreadProcessId(
            $window, [ref]$owner)
        if ($owner -eq $ProcessId) {
            $name = [Text.StringBuilder]::new(128)
            [void][AudioCommanderV6SettingsNative]::GetClassNameW(
                $window, $name, $name.Capacity)
            $windows.Add([pscustomobject]@{
                Handle = $window
                Class = $name.ToString()
            })
        }
        return $true
    }
    [void][AudioCommanderV6SettingsNative]::EnumWindows(
        $callback, [IntPtr]::Zero)
    $match = $windows | Where-Object { $_.Class -eq $Class } |
        Select-Object -First 1
    if ($match) { return $match.Handle }
    return [IntPtr]::Zero
}

function Wait-ForWindowByClass {
    param([int]$ProcessId, [string]$Class, [int]$TimeoutMilliseconds = 8000)

    $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMilliseconds)
    do {
        $window = Get-WindowByClass -ProcessId $ProcessId -Class $Class
        if ($window -ne [IntPtr]::Zero) { return $window }
        Start-Sleep -Milliseconds 50
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Timed out waiting for $Class."
}

function Get-ControlText {
    param([IntPtr]$Control)

    $text = [Text.StringBuilder]::new(1024)
    [void][AudioCommanderV6SettingsNative]::SendMessageTextW(
        $Control, 0x000D, [IntPtr]$text.Capacity, $text)
    return $text.ToString()
}

function Test-ContainedRectangle {
    param([IntPtr]$Outer, [IntPtr]$Inner, [string]$Description)

    $outerRect = New-Object AudioCommanderV6SettingsNative+Rect
    $innerRect = New-Object AudioCommanderV6SettingsNative+Rect
    if (-not [AudioCommanderV6SettingsNative]::GetWindowRect(
            $Outer, [ref]$outerRect) -or
        -not [AudioCommanderV6SettingsNative]::GetWindowRect(
            $Inner, [ref]$innerRect)) {
        throw "Could not measure $Description."
    }
    if ($innerRect.Left -lt $outerRect.Left -or
        $innerRect.Top -lt $outerRect.Top -or
        $innerRect.Right -gt $outerRect.Right -or
        $innerRect.Bottom -gt $outerRect.Bottom) {
        throw "$Description extends outside the Settings window."
    }
}

function Save-PrintedWindow {
    param([IntPtr]$Window, [string]$Path)

    Start-Sleep -Milliseconds 250
    $rectangle = New-Object AudioCommanderV6SettingsNative+Rect
    if (-not [AudioCommanderV6SettingsNative]::GetWindowRect(
            $Window, [ref]$rectangle)) {
        throw 'GetWindowRect failed for Settings.'
    }
    $width = $rectangle.Right - $rectangle.Left
    $height = $rectangle.Bottom - $rectangle.Top
    if ($width -lt 100 -or $height -lt 100) {
        throw "Settings window dimensions are invalid: ${width}x${height}."
    }
    $bitmap = [Drawing.Bitmap]::new($width, $height)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $deviceContext = $graphics.GetHdc()
    try {
        if (-not [AudioCommanderV6SettingsNative]::PrintWindow(
                $Window, $deviceContext, 0x00000002)) {
            throw 'PrintWindow failed for Settings.'
        }
    }
    finally {
        $graphics.ReleaseHdc($deviceContext)
        $graphics.Dispose()
    }
    try {
        $bitmap.Save($Path, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $bitmap.Dispose()
    }
}

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$runtimeDirectory = Split-Path -Parent $resolvedExecutable
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$resolvedOutput = (Resolve-Path -LiteralPath $OutputDirectory).Path
$temporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$fixture = [IO.Path]::GetFullPath((Join-Path $temporaryRoot (
    'AudioCommander-v6-settings-' + [Guid]::NewGuid().ToString('N'))))
if (-not $fixture.StartsWith($temporaryRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Refusing to create a Settings fixture outside the temporary directory.'
}

$cases = @(75, 100, 200)
$app = $null
try {
    New-Item -ItemType Directory -Path $fixture | Out-Null
    Copy-Item -LiteralPath $resolvedExecutable -Destination $fixture
    foreach ($library in Get-ChildItem -LiteralPath $runtimeDirectory -Filter '*.dll') {
        Copy-Item -LiteralPath $library.FullName -Destination $fixture
    }
    $fixtureExecutable = Join-Path $fixture (Split-Path -Leaf $resolvedExecutable)
    $ini = Join-Path $fixture 'audiocommander.ini'

    $results = foreach ($scale in $cases) {
        [IO.File]::WriteAllLines(
            $ini,
            @(
                '[Legal]', 'Accepted=1',
                '[Appearance]', 'Opacity=100', 'Theme=1', 'Language=0',
                "InterfaceSize=$scale",
                '[Folders]', 'Remember=0',
                '[Playback]', 'Sequential=0', 'Volume=0',
                '[Window]', 'RememberPosition=0'
            ),
            [Text.Encoding]::Unicode)
        $main = [IntPtr]::Zero
        $settings = [IntPtr]::Zero
        $app = Start-Process -FilePath $fixtureExecutable `
            -WorkingDirectory $fixture -PassThru
        try {
            $main = Wait-ForWindowByClass -ProcessId $app.Id `
                -Class 'AudioCommanderWindow'
            [void][AudioCommanderV6SettingsNative]::PostMessageW(
                $main, 0x0111, [IntPtr]10, [IntPtr]::Zero)
            $settings = Wait-ForWindowByClass -ProcessId $app.Id `
                -Class 'AudioCommanderSettings'
            $remember = [AudioCommanderV6SettingsNative]::GetDlgItem(
                $settings, 310)
            $ok = [AudioCommanderV6SettingsNative]::GetDlgItem($settings, 308)
            $cancel = [AudioCommanderV6SettingsNative]::GetDlgItem($settings, 309)
            if ($remember -eq [IntPtr]::Zero -or $ok -eq [IntPtr]::Zero -or
                $cancel -eq [IntPtr]::Zero) {
                throw "Required Settings controls were missing at $scale percent."
            }
            $label = Get-ControlText $remember
            if ($label -ne 'Remember last used directories (left and right panes)') {
                throw "Unexpected directory-memory label at $scale percent: $label"
            }
            if ([AudioCommanderV6SettingsNative]::SendMessageW(
                    $remember, 0x00F0, [IntPtr]::Zero,
                    [IntPtr]::Zero).ToInt64() -ne 0) {
                throw "Directory memory was not off by default at $scale percent."
            }
            Test-ContainedRectangle $settings $remember `
                "Directory-memory checkbox at $scale percent"
            Test-ContainedRectangle $settings $ok "OK button at $scale percent"
            Test-ContainedRectangle $settings $cancel "Cancel button at $scale percent"
            $imagePath = Join-Path $resolvedOutput (
                "settings-scale-$scale-english.png")
            Save-PrintedWindow -Window $settings -Path $imagePath
            [pscustomobject]@{
                Scale = $scale
                CheckboxLabel = 'passed'
                DefaultOff = 'passed'
                Bounds = 'passed'
                Screenshot = $imagePath
            }
        }
        finally {
            if ($settings -ne [IntPtr]::Zero) {
                [void][AudioCommanderV6SettingsNative]::SendMessageW(
                    $settings, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
            }
            if ($app -and -not $app.HasExited) {
                if ($main -ne [IntPtr]::Zero) {
                    [void][AudioCommanderV6SettingsNative]::PostMessageW(
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
        $cleanup = [IO.Path]::GetFullPath($fixture)
        if (-not $cleanup.StartsWith(
                $temporaryRoot, [StringComparison]::OrdinalIgnoreCase) -or
            -not (Split-Path -Leaf $cleanup).StartsWith(
                'AudioCommander-v6-settings-', [StringComparison]::Ordinal)) {
            throw "Refusing unsafe Settings-fixture cleanup path: $cleanup"
        }
        Remove-Item -LiteralPath $cleanup -Recurse -Force
    }
}
