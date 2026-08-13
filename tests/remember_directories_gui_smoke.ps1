param(
    [string]$Executable = (Join-Path $PSScriptRoot '..\build-v6-release\audiocommander_v6.exe')
)

$ErrorActionPreference = 'Stop'
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class AudioDirectoryNative
{
    public delegate bool EnumWindowProc(IntPtr window, IntPtr parameter);

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
    public static extern IntPtr SendMessageW(
        IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "SendMessageW")]
    public static extern IntPtr SendMessageTextW(
        IntPtr window, uint message, IntPtr wParam, StringBuilder lParam);

    [DllImport("user32.dll")]
    public static extern bool PostMessageW(
        IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool IsWindowEnabled(IntPtr window);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    public static extern uint GetPrivateProfileIntW(
        string section, string key, int defaultValue, string fileName);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    public static extern uint GetPrivateProfileStringW(
        string section, string key, string defaultValue,
        StringBuilder value, uint capacity, string fileName);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    public static extern bool WritePrivateProfileStringW(
        string section, string key, string value, string fileName);
}
'@

function Get-TopWindows {
    param([int]$OwnerProcessId)

    $windows = [Collections.Generic.List[object]]::new()
    $callback = [AudioDirectoryNative+EnumWindowProc]{
        param([IntPtr]$window, [IntPtr]$parameter)
        [uint32]$owner = 0
        [void][AudioDirectoryNative]::GetWindowThreadProcessId($window, [ref]$owner)
        if ($owner -eq $OwnerProcessId) {
            $class = [Text.StringBuilder]::new(128)
            $title = [Text.StringBuilder]::new(512)
            [void][AudioDirectoryNative]::GetClassNameW(
                $window, $class, $class.Capacity)
            [void][AudioDirectoryNative]::GetWindowTextW(
                $window, $title, $title.Capacity)
            $windows.Add([pscustomobject]@{
                Handle = $window
                Class = $class.ToString()
                Title = $title.ToString()
            })
        }
        return $true
    }
    [void][AudioDirectoryNative]::EnumWindows($callback, [IntPtr]::Zero)
    return @($windows)
}

function Wait-ForWindow {
    param([int]$OwnerProcessId, [string]$Class, [int]$TimeoutMilliseconds = 8000)

    $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMilliseconds)
    do {
        $match = Get-TopWindows -OwnerProcessId $OwnerProcessId |
            Where-Object { $_.Class -eq $Class } |
            Select-Object -First 1
        if ($match) { return $match }
        Start-Sleep -Milliseconds 50
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Timed out waiting for $Class."
}

function Get-ControlText {
    param([IntPtr]$Control)

    $text = [Text.StringBuilder]::new(32768)
    [void][AudioDirectoryNative]::SendMessageTextW(
        $Control, 0x000D, [IntPtr]$text.Capacity, $text)
    return $text.ToString()
}

function Get-IniText {
    param([string]$Ini, [string]$Key)

    $text = [Text.StringBuilder]::new(32768)
    [void][AudioDirectoryNative]::GetPrivateProfileStringW(
        'Folders', $Key, '', $text, $text.Capacity, $Ini)
    return $text.ToString()
}

function Start-TestApp {
    param([string]$ExecutablePath, [string]$WorkingDirectory)

    $process = Start-Process -FilePath $ExecutablePath `
        -WorkingDirectory $WorkingDirectory -PassThru
    $script:CurrentTestProcess = $process
    $main = Wait-ForWindow -OwnerProcessId $process.Id -Class 'AudioCommanderWindow'
    $deadline = [DateTime]::UtcNow.AddSeconds(5)
    do {
        $leftHandle = [AudioDirectoryNative]::GetDlgItem($main.Handle, 100)
        $rightHandle = [AudioDirectoryNative]::GetDlgItem($main.Handle, 200)
        $leftPath = Get-ControlText $leftHandle
        $rightPath = Get-ControlText $rightHandle
        if ($leftPath -ne '' -and $rightPath -ne '') { break }
        Start-Sleep -Milliseconds 50
    } while ([DateTime]::UtcNow -lt $deadline)
    if ($leftPath -eq '' -or $rightPath -eq '') {
        $topDescription = (Get-TopWindows -OwnerProcessId $process.Id |
            ForEach-Object { "$($_.Class)[$($_.Title)]" }) -join ', '
        $leftClass = [Text.StringBuilder]::new(64)
        $rightClass = [Text.StringBuilder]::new(64)
        [void][AudioDirectoryNative]::GetClassNameW(
            $leftHandle, $leftClass, $leftClass.Capacity)
        [void][AudioDirectoryNative]::GetClassNameW(
            $rightHandle, $rightClass, $rightClass.Capacity)
        throw "Pane paths empty. left=$leftHandle/$leftClass right=$rightHandle/$rightClass. Top windows: $topDescription"
    }
    return [pscustomobject]@{ Process = $process; Main = $main.Handle }
}

function Stop-TestApp {
    param($App)

    if (-not $App -and $script:CurrentTestProcess -and
        -not $script:CurrentTestProcess.HasExited) {
        Stop-Process -Id $script:CurrentTestProcess.Id -Force -ErrorAction SilentlyContinue
        [void]$script:CurrentTestProcess.WaitForExit(3000)
        $script:CurrentTestProcess = $null
        return
    }
    if ($App -and -not $App.Process.HasExited) {
        [void][AudioDirectoryNative]::PostMessageW(
            $App.Main, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
        [void]$App.Process.WaitForExit(4000)
        if (-not $App.Process.HasExited) {
            Stop-Process -Id $App.Process.Id -Force -ErrorAction SilentlyContinue
            [void]$App.Process.WaitForExit(3000)
        }
    }
    $script:CurrentTestProcess = $null
}

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$runtimeDirectory = Split-Path -Parent $resolvedExecutable
$temporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$fixture = [IO.Path]::GetFullPath((Join-Path $temporaryRoot (
    'AudioCommander-directories-' + [Guid]::NewGuid().ToString('N'))))
if (-not $fixture.StartsWith($temporaryRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Refusing to create a directory-memory fixture outside the temporary directory.'
}

$app = $null
$script:CurrentTestProcess = $null
try {
    $left = Join-Path $fixture 'left-audio'
    $right = Join-Path $fixture 'right-audio'
    New-Item -ItemType Directory -Path $left, $right | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $left 'album-a'), `
        (Join-Path $right 'album-b') | Out-Null
    Copy-Item -LiteralPath $resolvedExecutable -Destination $fixture
    foreach ($library in Get-ChildItem -LiteralPath $runtimeDirectory -Filter '*.dll') {
        Copy-Item -LiteralPath $library.FullName -Destination $fixture
    }
    $fixtureExecutable = Join-Path $fixture (Split-Path -Leaf $resolvedExecutable)
    $ini = Join-Path $fixture 'audiocommander.ini'
    [IO.File]::WriteAllLines(
        $ini,
        @(
            '[Legal]', 'Accepted=1',
            '[Appearance]', 'Opacity=100', 'Language=0', 'InterfaceSize=100',
            '[Folders]', 'Remember=1', "Left=$left", "Right=$right",
            '[Playback]', 'Sequential=0', 'Volume=0'
        ),
        [Text.Encoding]::Unicode)

    Write-Output 'Phase: independent left/right restore'
    $app = Start-TestApp -ExecutablePath $fixtureExecutable -WorkingDirectory $fixture
    $leftActual = Get-ControlText ([AudioDirectoryNative]::GetDlgItem($app.Main, 100))
    $rightActual = Get-ControlText ([AudioDirectoryNative]::GetDlgItem($app.Main, 200))
    if ($leftActual -ne $left -or $rightActual -ne $right) {
        throw "Initial restore mismatch: left=$leftActual right=$rightActual"
    }
    Stop-TestApp $app
    $app = $null

    $missing = Join-Path $fixture 'missing-right'
    if (-not [AudioDirectoryNative]::WritePrivateProfileStringW(
            'Folders', 'Right', $missing, $ini)) {
        throw 'Could not prepare the unavailable-folder case.'
    }
    Write-Output 'Phase: unavailable right-folder fallback'
    $app = Start-TestApp -ExecutablePath $fixtureExecutable -WorkingDirectory $fixture
    $leftActual = Get-ControlText ([AudioDirectoryNative]::GetDlgItem($app.Main, 100))
    $rightActual = Get-ControlText ([AudioDirectoryNative]::GetDlgItem($app.Main, 200))
    if ($leftActual -ne $left -or $rightActual -ne $fixture) {
        throw "Fallback mismatch: left=$leftActual right=$rightActual"
    }

    Write-Output 'Phase: Settings checkbox and clearing saved paths'
    [void][AudioDirectoryNative]::PostMessageW(
        $app.Main, 0x0111, [IntPtr]10, [IntPtr]::Zero)
    $settings = Wait-ForWindow -OwnerProcessId $app.Process.Id `
        -Class 'AudioCommanderSettings'
    $remember = [AudioDirectoryNative]::GetDlgItem($settings.Handle, 310)
    if ($remember -eq [IntPtr]::Zero -or
        (Get-ControlText $remember) -ne
            'Remember last used directories (left and right panes)' -or
        [AudioDirectoryNative]::SendMessageW(
            $remember, 0x00F0, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64() -ne 1) {
        throw 'The checked remember-directories Settings control was not found.'
    }
    [void][AudioDirectoryNative]::SendMessageW(
        $remember, 0x00F1, [IntPtr]::Zero, [IntPtr]::Zero)
    $ok = [AudioDirectoryNative]::GetDlgItem($settings.Handle, 308)
    [void][AudioDirectoryNative]::SendMessageW(
        $ok, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    [void][AudioDirectoryNative]::PostMessageW(
        $app.Main, 0x0000, [IntPtr]::Zero, [IntPtr]::Zero)
    $deadline = [DateTime]::UtcNow.AddSeconds(3)
    while (-not [AudioDirectoryNative]::IsWindowEnabled($app.Main) -and
           [DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 25
    }
    if (-not [AudioDirectoryNative]::IsWindowEnabled($app.Main)) {
        throw 'The main window was not re-enabled after Settings.'
    }
    $deadline = [DateTime]::UtcNow.AddSeconds(3)
    do {
        $rememberValue = [AudioDirectoryNative]::GetPrivateProfileIntW(
            'Folders', 'Remember', -1, $ini)
        $leftStored = Get-IniText -Ini $ini -Key 'Left'
        $rightStored = Get-IniText -Ini $ini -Key 'Right'
        if ($rememberValue -eq 0 -and $leftStored -eq '' -and
            $rightStored -eq '') { break }
        Start-Sleep -Milliseconds 25
    } while ([DateTime]::UtcNow -lt $deadline)
    if ($rememberValue -ne 0 -or $leftStored -ne '' -or $rightStored -ne '') {
        throw 'Disabling directory memory did not clear both stored paths.'
    }
    Stop-TestApp $app
    $app = $null

    Write-Output 'Phase: disabled setting uses launch directory'
    $app = Start-TestApp -ExecutablePath $fixtureExecutable -WorkingDirectory $fixture
    $leftActual = Get-ControlText ([AudioDirectoryNative]::GetDlgItem($app.Main, 100))
    $rightActual = Get-ControlText ([AudioDirectoryNative]::GetDlgItem($app.Main, 200))
    if ($leftActual -ne $fixture -or $rightActual -ne $fixture) {
        throw "Disabled-setting startup mismatch: left=$leftActual right=$rightActual"
    }
    Stop-TestApp $app
    $app = $null

    [pscustomobject]@{
        IndependentLeftRightRestore = 'passed'
        MissingFolderFallback = 'passed'
        SettingsCheckbox = 'passed'
        DisableClearsSavedPaths = 'passed'
        DisabledStartupUsesLaunchDirectory = 'passed'
    }
}
finally {
    Stop-TestApp $app
    Stop-TestApp $null
    if (Test-Path -LiteralPath $fixture) {
        $cleanup = [IO.Path]::GetFullPath($fixture)
        if (-not $cleanup.StartsWith(
                $temporaryRoot, [StringComparison]::OrdinalIgnoreCase) -or
            -not (Split-Path -Leaf $cleanup).StartsWith(
                'AudioCommander-directories-', [StringComparison]::Ordinal)) {
            throw "Refusing unsafe fixture cleanup path: $cleanup"
        }
        $cleanupError = $null
        for ($attempt = 0; $attempt -lt 20; ++$attempt) {
            try {
                Remove-Item -LiteralPath $cleanup -Recurse -Force -ErrorAction Stop
                $cleanupError = $null
                break
            }
            catch {
                $cleanupError = $_
                Start-Sleep -Milliseconds 100
            }
        }
        if ($cleanupError) { throw $cleanupError }
    }
}
