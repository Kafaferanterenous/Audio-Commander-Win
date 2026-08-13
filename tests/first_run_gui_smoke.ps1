param(
    [string]$Executable = (Join-Path $PSScriptRoot '..\build-v4-nmake\audiocommander_v4.exe'),
    [string]$Screenshot = (Join-Path $PSScriptRoot '..\build-v4-nmake\live-gui\first-run-notice.png')
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class AudioCommanderFirstRunNative
{
    public delegate bool EnumWindowProc(IntPtr window, IntPtr parameter);

    [StructLayout(LayoutKind.Sequential)]
    public struct Rect
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowProc callback, IntPtr parameter);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassNameW(IntPtr window, StringBuilder text, int capacity);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowTextW(IntPtr window, StringBuilder text, int capacity);

    [DllImport("user32.dll")]
    public static extern IntPtr GetDlgItem(IntPtr dialog, int controlId);

    [DllImport("user32.dll")]
    public static extern IntPtr SendMessageW(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool PostMessageW(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr window, out Rect rectangle);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr window);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr window, int command);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    public static extern uint GetPrivateProfileIntW(
        string section, string key, int defaultValue, string fileName);
}
'@

function Get-ProcessWindows {
    param([int]$ProcessId)

    $windows = [System.Collections.Generic.List[object]]::new()
    $callback = [AudioCommanderFirstRunNative+EnumWindowProc]{
        param([IntPtr]$window, [IntPtr]$parameter)
        [uint32]$ownerProcess = 0
        [void][AudioCommanderFirstRunNative]::GetWindowThreadProcessId(
            $window, [ref]$ownerProcess)
        if ($ownerProcess -eq $ProcessId) {
            $classText = [Text.StringBuilder]::new(128)
            $titleText = [Text.StringBuilder]::new(512)
            [void][AudioCommanderFirstRunNative]::GetClassNameW(
                $window, $classText, $classText.Capacity)
            [void][AudioCommanderFirstRunNative]::GetWindowTextW(
                $window, $titleText, $titleText.Capacity)
            $windows.Add([pscustomobject]@{
                Handle = $window
                Class = $classText.ToString()
                Title = $titleText.ToString()
            })
        }
        return $true
    }
    [void][AudioCommanderFirstRunNative]::EnumWindows($callback, [IntPtr]::Zero)
    return @($windows)
}

function Wait-ForWindow {
    param(
        [int]$ProcessId,
        [string]$Class,
        [int]$TimeoutMilliseconds = 5000
    )

    $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMilliseconds)
    do {
        $match = Get-ProcessWindows -ProcessId $ProcessId |
            Where-Object { $_.Class -eq $Class } |
            Select-Object -First 1
        if ($match) {
            return $match
        }
        Start-Sleep -Milliseconds 50
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Timed out waiting for $Class in process $ProcessId."
}

function Click-DialogButton {
    param(
        [IntPtr]$Dialog,
        [int]$ButtonId
    )

    $button = [AudioCommanderFirstRunNative]::GetDlgItem($Dialog, $ButtonId)
    if ($button -eq [IntPtr]::Zero) {
        throw "Dialog button $ButtonId was not found."
    }
    [void][AudioCommanderFirstRunNative]::SendMessageW(
        $button, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
}

function Save-WindowImage {
    param(
        [IntPtr]$Window,
        [string]$Path
    )

    $parent = Split-Path -Parent $Path
    if ($parent) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
    [void][AudioCommanderFirstRunNative]::ShowWindow($Window, 9)
    [void][AudioCommanderFirstRunNative]::SetForegroundWindow($Window)
    Start-Sleep -Milliseconds 200
    $rectangle = New-Object AudioCommanderFirstRunNative+Rect
    if (-not [AudioCommanderFirstRunNative]::GetWindowRect($Window, [ref]$rectangle)) {
        throw 'GetWindowRect failed for the first-run notice.'
    }
    $width = $rectangle.Right - $rectangle.Left
    $height = $rectangle.Bottom - $rectangle.Top
    $bitmap = [Drawing.Bitmap]::new($width, $height)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen(
            $rectangle.Left, $rectangle.Top, 0, 0,
            [Drawing.Size]::new($width, $height))
        $bitmap.Save($Path, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$tempBase = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$fixture = Join-Path $tempBase ('AudioCommander-first-run-' + [guid]::NewGuid().ToString('N'))
$fixture = [IO.Path]::GetFullPath($fixture)
if (-not $fixture.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Temporary fixture escaped the system temporary directory.'
}

$processes = [System.Collections.Generic.List[Diagnostics.Process]]::new()
try {
    New-Item -ItemType Directory -Path $fixture | Out-Null
    $fixtureExecutable = Join-Path $fixture 'audiocommander_v4.exe'
    $fixtureIni = Join-Path $fixture 'audiocommander.ini'
    Copy-Item -LiteralPath $resolvedExecutable -Destination $fixtureExecutable
    foreach ($dependency in @(
        'avcodec-63.dll',
        'avformat-63.dll',
        'avutil-61.dll',
        'swresample-7.dll'
    )) {
        $dependencyPath = Join-Path (Split-Path -Parent $resolvedExecutable) $dependency
        if (-not (Test-Path -LiteralPath $dependencyPath)) {
            throw "Required test dependency is missing: $dependencyPath"
        }
        Copy-Item -LiteralPath $dependencyPath -Destination (Join-Path $fixture $dependency)
    }

    $decline = Start-Process -FilePath $fixtureExecutable -WorkingDirectory $fixture -PassThru
    $processes.Add($decline)
    $notice = Wait-ForWindow -ProcessId $decline.Id -Class '#32770'
    if ($notice.Title -ne 'First-run notice') {
        throw "Unexpected first-run title: $($notice.Title)"
    }
    Save-WindowImage -Window $notice.Handle -Path $Screenshot
    if (Test-Path -LiteralPath $fixtureIni) {
        throw 'The INI existed before the user made a choice.'
    }
    Click-DialogButton -Dialog $notice.Handle -ButtonId 7
    if (-not $decline.WaitForExit(3000)) {
        throw 'The No branch did not exit.'
    }
    if (Test-Path -LiteralPath $fixtureIni) {
        throw 'The No branch wrote an INI file.'
    }

    $localizedCases = @(
        [pscustomobject]@{
            Language = 1
            Title = ([char[]](0x9996, 0x6B21, 0x8FD0, 0x884C, 0x987B, 0x77E5) -join '')
            Slug = 'chinese'
        },
        [pscustomobject]@{
            Language = 2
            Title = 'Avviso al primo avvio'
            Slug = 'italian'
        },
        [pscustomobject]@{
            Language = 3
            Title = 'Informacja przy pierwszym uruchomieniu'
            Slug = 'polish'
        }
    )
    foreach ($case in $localizedCases) {
        [IO.File]::WriteAllText(
            $fixtureIni,
            "[Appearance]`r`nVersion=3`r`nLanguage=$($case.Language)`r`nTestSentinel=unchanged`r`n",
            [Text.UTF8Encoding]::new($false))
        $existingIniBefore = (Get-FileHash -LiteralPath $fixtureIni -Algorithm SHA256).Hash
        $declineExisting = Start-Process -FilePath $fixtureExecutable -WorkingDirectory $fixture -PassThru
        $processes.Add($declineExisting)
        $notice = Wait-ForWindow -ProcessId $declineExisting.Id -Class '#32770'
        if ($notice.Title -ne $case.Title) {
            throw "Unexpected $($case.Slug) first-run title: $($notice.Title)"
        }
        $localizedScreenshot = Join-Path (Split-Path -Parent $Screenshot) `
            "first-run-notice-$($case.Slug).png"
        Save-WindowImage -Window $notice.Handle -Path $localizedScreenshot
        Click-DialogButton -Dialog $notice.Handle -ButtonId 7
        if (-not $declineExisting.WaitForExit(3000)) {
            throw "The $($case.Slug) existing-INI No branch did not exit."
        }
        $existingIniAfter = (Get-FileHash -LiteralPath $fixtureIni -Algorithm SHA256).Hash
        if ($existingIniBefore -ne $existingIniAfter) {
            throw "The $($case.Slug) existing-INI No branch modified the INI."
        }
    }

    $accept = Start-Process -FilePath $fixtureExecutable -WorkingDirectory $fixture -PassThru
    $processes.Add($accept)
    $notice = Wait-ForWindow -ProcessId $accept.Id -Class '#32770'
    Click-DialogButton -Dialog $notice.Handle -ButtonId 6
    $main = Wait-ForWindow -ProcessId $accept.Id -Class 'AudioCommanderWindow'
    $accepted = [AudioCommanderFirstRunNative]::GetPrivateProfileIntW(
        'Legal', 'Accepted', 0, $fixtureIni)
    if ($accepted -ne 1) {
        throw "The Yes branch stored Accepted=$accepted instead of 1."
    }
    [void][AudioCommanderFirstRunNative]::PostMessageW(
        $main.Handle, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
    if (-not $accept.WaitForExit(3000)) {
        throw 'The accepted first launch did not close.'
    }

    $repeat = Start-Process -FilePath $fixtureExecutable -WorkingDirectory $fixture -PassThru
    $processes.Add($repeat)
    $main = Wait-ForWindow -ProcessId $repeat.Id -Class 'AudioCommanderWindow'
    if (Get-ProcessWindows -ProcessId $repeat.Id |
        Where-Object { $_.Class -eq '#32770' -and $_.Title -eq 'First-run notice' }) {
        throw 'The notice appeared again after acceptance.'
    }
    [void][AudioCommanderFirstRunNative]::PostMessageW(
        $main.Handle, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
    if (-not $repeat.WaitForExit(3000)) {
        throw 'The repeat launch did not close.'
    }

    [pscustomobject]@{
        NoBranchWroteIni = $false
        ExistingIniNoBranchExact = ($existingIniBefore -eq $existingIniAfter)
        LocalizedNoticeTitles = 'Chinese, Italian, Polish passed'
        YesBranchAcceptedValue = $accepted
        RepeatLaunchSkippedNotice = $true
    }
}
finally {
    foreach ($process in $processes) {
        if (-not $process.HasExited) {
            Stop-Process -Id $process.Id -ErrorAction SilentlyContinue
        }
    }
    if (Test-Path -LiteralPath $fixture) {
        $resolvedFixture = [IO.Path]::GetFullPath((Resolve-Path -LiteralPath $fixture).Path)
        if (-not $resolvedFixture.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Refusing to remove a fixture outside the system temporary directory.'
        }
        Remove-Item -LiteralPath $resolvedFixture -Recurse -Force
    }
}
