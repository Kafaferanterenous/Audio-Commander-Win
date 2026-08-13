param(
    [string]$Executable = (Join-Path $PSScriptRoot '..\build-v4-nmake\audiocommander_v4.exe'),
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\build-v4-nmake\live-gui'),
    [string]$ExpectedItalianHelpTitle = 'Guida di AudioCommander v4',
    [int]$SettingsControlOffset = 0,
    [switch]$ExpectV5Controls,
    [switch]$ExpectMidiFixture,
    [switch]$OpaqueLayouts,
    [switch]$SkipScreenshots
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class AudioCommanderGuiNative
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
    public static extern bool EnumChildWindows(IntPtr parent, EnumWindowProc callback, IntPtr parameter);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassNameW(IntPtr window, StringBuilder text, int capacity);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowTextW(IntPtr window, StringBuilder text, int capacity);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr window);

    [DllImport("user32.dll")]
    public static extern bool IsWindowEnabled(IntPtr window);

    [DllImport("user32.dll")]
    public static extern bool PostMessageW(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern IntPtr SendMessageW(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr SendDlgItemMessageW(
        IntPtr dialog, int controlId, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern bool SetDlgItemTextW(IntPtr dialog, int controlId, string text);

    [DllImport("user32.dll")]
    public static extern bool SetDlgItemInt(
        IntPtr dialog, int controlId, uint value, bool signedValue);

    [DllImport("user32.dll")]
    public static extern IntPtr GetDlgItem(IntPtr dialog, int controlId);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern bool SetWindowTextW(IntPtr window, string text);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr window, out Rect rectangle);

    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr window, IntPtr deviceContext, uint flags);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr window);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr window, int command);

    [DllImport("user32.dll")]
    public static extern int GetWindowLongW(IntPtr window, int index);

    [DllImport("user32.dll")]
    public static extern bool GetLayeredWindowAttributes(
        IntPtr window, out uint colorKey, out byte alpha, out uint flags);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    public static extern uint GetPrivateProfileIntW(
        string section, string key, int defaultValue, string fileName);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    public static extern bool WritePrivateProfileStringW(
        string section, string key, string value, string fileName);
}
'@

function Get-TopWindows {
    param([int]$ProcessId)

    $windows = [System.Collections.Generic.List[object]]::new()
    $callback = [AudioCommanderGuiNative+EnumWindowProc]{
        param([IntPtr]$window, [IntPtr]$parameter)
        [uint32]$ownerProcess = 0
        [void][AudioCommanderGuiNative]::GetWindowThreadProcessId($window, [ref]$ownerProcess)
        if ($ownerProcess -eq $ProcessId) {
            $classText = [Text.StringBuilder]::new(128)
            $titleText = [Text.StringBuilder]::new(512)
            [void][AudioCommanderGuiNative]::GetClassNameW($window, $classText, $classText.Capacity)
            [void][AudioCommanderGuiNative]::GetWindowTextW($window, $titleText, $titleText.Capacity)
            $windows.Add([pscustomobject]@{
                Handle = $window
                Class = $classText.ToString()
                Title = $titleText.ToString()
                Visible = [AudioCommanderGuiNative]::IsWindowVisible($window)
            })
        }
        return $true
    }
    [void][AudioCommanderGuiNative]::EnumWindows($callback, [IntPtr]::Zero)
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
        $match = Get-TopWindows -ProcessId $ProcessId |
            Where-Object { $_.Class -eq $Class } |
            Select-Object -First 1
        if ($match) {
            return $match
        }
        Start-Sleep -Milliseconds 50
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Timed out waiting for window class $Class in process $ProcessId."
}

function Get-ChildTexts {
    param([IntPtr]$Parent)

    $texts = [System.Collections.Generic.List[string]]::new()
    $callback = [AudioCommanderGuiNative+EnumWindowProc]{
        param([IntPtr]$window, [IntPtr]$parameter)
        $text = [Text.StringBuilder]::new(512)
        [void][AudioCommanderGuiNative]::GetWindowTextW($window, $text, $text.Capacity)
        if ($text.Length -gt 0) {
            $texts.Add($text.ToString())
        }
        return $true
    }
    [void][AudioCommanderGuiNative]::EnumChildWindows($Parent, $callback, [IntPtr]::Zero)
    return @($texts)
}

function Set-AppSettings {
    param(
        [IntPtr]$MainWindow,
        [int]$ProcessId,
        [int]$Theme,
        [int]$Language
    )

    $retainedOpacity = [AudioCommanderGuiNative]::GetPrivateProfileIntW(
        'Appearance', 'Opacity', 100, $script:SettingsIniPath)
    [void][AudioCommanderGuiNative]::PostMessageW(
        $MainWindow, 0x0111, [IntPtr]10, [IntPtr]::Zero)
    $settings = Wait-ForWindow -ProcessId $ProcessId -Class 'AudioCommanderSettings'

    if ($ExpectV5Controls) {
        $settingsTexts = Get-ChildTexts -Parent $settings.Handle
        if ($settingsTexts -notcontains 'Play next file automatically') {
            throw 'The v5 sequential-playback checkbox was not found in Settings.'
        }
    }
    $themeResult = [AudioCommanderGuiNative]::SendDlgItemMessageW(
        $settings.Handle, (305 + $SettingsControlOffset), 0x014E,
        [IntPtr]$Theme, [IntPtr]::Zero).ToInt64()
    $languageResult = [AudioCommanderGuiNative]::SendDlgItemMessageW(
        $settings.Handle, (306 + $SettingsControlOffset), 0x014E,
        [IntPtr]$Language, [IntPtr]::Zero).ToInt64()
    if ($themeResult -ne $Theme -or $languageResult -ne $Language) {
        throw "Settings combo selection failed (theme=$themeResult, language=$languageResult)."
    }
    $okButton = [AudioCommanderGuiNative]::GetDlgItem(
        $settings.Handle, (307 + $SettingsControlOffset))
    if ($okButton -eq [IntPtr]::Zero) {
        throw 'Settings OK button was not found.'
    }
    [void][AudioCommanderGuiNative]::SendMessageW(
        $okButton, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    # A cross-process SendMessage closes the modal window outside its nested
    # GetMessage loop. Queue WM_NULL so that loop wakes and applies the accepted
    # settings before this function inspects the main window.
    [void][AudioCommanderGuiNative]::PostMessageW(
        $MainWindow, 0x0000, [IntPtr]::Zero, [IntPtr]::Zero)
    $enableDeadline = [DateTime]::UtcNow.AddSeconds(3)
    while (-not [AudioCommanderGuiNative]::IsWindowEnabled($MainWindow) -and
           [DateTime]::UtcNow -lt $enableDeadline) {
        Start-Sleep -Milliseconds 25
    }
    if (-not [AudioCommanderGuiNative]::IsWindowEnabled($MainWindow)) {
        throw 'Main window was not re-enabled after Settings closed.'
    }
    $persistDeadline = [DateTime]::UtcNow.AddSeconds(3)
    do {
        $savedTheme = [AudioCommanderGuiNative]::GetPrivateProfileIntW(
            'Appearance', 'Theme', -1, $script:SettingsIniPath)
        $savedLanguage = [AudioCommanderGuiNative]::GetPrivateProfileIntW(
            'Appearance', 'Language', -1, $script:SettingsIniPath)
        $savedOpacity = [AudioCommanderGuiNative]::GetPrivateProfileIntW(
            'Appearance', 'Opacity', -1, $script:SettingsIniPath)
        if ($savedTheme -eq $Theme -and $savedLanguage -eq $Language -and
            $savedOpacity -eq $retainedOpacity) {
            break
        }
        [void][AudioCommanderGuiNative]::PostMessageW(
            $MainWindow, 0x0000, [IntPtr]::Zero, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 25
    } while ([DateTime]::UtcNow -lt $persistDeadline)
    if ($savedTheme -ne $Theme -or $savedLanguage -ne $Language -or
        $savedOpacity -ne $retainedOpacity) {
        throw "Settings were not persisted (theme=$savedTheme, language=$savedLanguage, opacity=$savedOpacity)."
    }
    if (Get-TopWindows -ProcessId $ProcessId |
        Where-Object { $_.Class -eq 'AudioCommanderSettings' }) {
        throw 'Settings did not close after OK.'
    }
    Start-Sleep -Milliseconds 200
}

function Save-WindowImage {
    param(
        [IntPtr]$Window,
        [string]$Path
    )

    [void][AudioCommanderGuiNative]::ShowWindow($Window, 9)
    [void][AudioCommanderGuiNative]::SetForegroundWindow($Window)
    Start-Sleep -Milliseconds 200
    $rectangle = New-Object AudioCommanderGuiNative+Rect
    if (-not [AudioCommanderGuiNative]::GetWindowRect($Window, [ref]$rectangle)) {
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
                if (-not [AudioCommanderGuiNative]::PrintWindow(
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
$script:SettingsIniPath = Join-Path (Split-Path -Parent $resolvedExecutable) 'audiocommander.ini'
$originalIniExists = Test-Path -LiteralPath $script:SettingsIniPath
$originalIniBytes = if ($originalIniExists) {
    [IO.File]::ReadAllBytes($script:SettingsIniPath)
}
else {
    $null
}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$output = (Resolve-Path -LiteralPath $OutputDirectory).Path
$themeNames = @('follow-windows', 'light', 'dark', 'blue', 'pastel')
$app = $null
$second = $null
$originalTheme = [AudioCommanderGuiNative]::GetPrivateProfileIntW(
    'Appearance', 'Theme', 0, $script:SettingsIniPath)
$originalLanguage = [AudioCommanderGuiNative]::GetPrivateProfileIntW(
    'Appearance', 'Language', 0, $script:SettingsIniPath)
$originalOpacity = [AudioCommanderGuiNative]::GetPrivateProfileIntW(
    'Appearance', 'Opacity', 100, $script:SettingsIniPath)
$acceptanceSentinel = 2147483646
$originalAcceptance = [AudioCommanderGuiNative]::GetPrivateProfileIntW(
    'Legal', 'Accepted', $acceptanceSentinel, $script:SettingsIniPath)

try {
    if (-not [AudioCommanderGuiNative]::WritePrivateProfileStringW(
        'Legal', 'Accepted', '1', $script:SettingsIniPath)) {
        throw 'Could not prepare the disposable first-run acceptance setting.'
    }
    $startupOpacity = if ($OpaqueLayouts) { '100' } else { '80' }
    if (-not [AudioCommanderGuiNative]::WritePrivateProfileStringW(
        'Appearance', 'Opacity', $startupOpacity, $script:SettingsIniPath)) {
        throw 'Could not prepare the disposable transparency setting.'
    }
    $app = Start-Process -FilePath $resolvedExecutable `
        -WorkingDirectory (Split-Path -Parent $resolvedExecutable) -PassThru
    $main = Wait-ForWindow -ProcessId $app.Id -Class 'AudioCommanderWindow'
    $transparencyDeadline = [DateTime]::UtcNow.AddSeconds(3)
    do {
        $startupStyle = [AudioCommanderGuiNative]::GetWindowLongW($main.Handle, -20)
        [uint32]$startupColorKey = 0
        [byte]$startupAlpha = 0
        [uint32]$startupLayerFlags = 0
        $startupHasLayerAttributes = [AudioCommanderGuiNative]::GetLayeredWindowAttributes(
            $main.Handle, [ref]$startupColorKey, [ref]$startupAlpha, [ref]$startupLayerFlags)
        if ($OpaqueLayouts -or
            ((($startupStyle -band 0x00080000) -ne 0) -and
             $startupHasLayerAttributes -and $startupAlpha -eq 204)) {
            break
        }
        Start-Sleep -Milliseconds 25
    } while ([DateTime]::UtcNow -lt $transparencyDeadline)
    if (-not $OpaqueLayouts) {
        if (($startupStyle -band 0x00080000) -eq 0 -or
            -not $startupHasLayerAttributes -or $startupAlpha -ne 204) {
            throw "Transparency validation failed (style=$startupStyle, alpha=$startupAlpha, flags=$startupLayerFlags)."
        }
        if (-not $SkipScreenshots) {
            Save-WindowImage -Window $main.Handle `
                -Path (Join-Path $output 'transparency-20-percent.png')
        }
    }
    Set-AppSettings -MainWindow $main.Handle -ProcessId $app.Id `
        -Theme 0 -Language 0
    $englishTexts = Get-ChildTexts -Parent $main.Handle
    foreach ($expected in @('Settings...', 'Stop', 'Swap', 'Refresh')) {
        if ($englishTexts -notcontains $expected) {
            throw "English control not found: $expected"
        }
    }
    if ($ExpectV5Controls) {
        if (($englishTexts | Where-Object { $_ -eq 'Browse...' }).Count -ne 2) {
            throw 'Expected one v5 Browse button in each pane.'
        }
        if (($englishTexts | Where-Object { $_ -match '^Audio:.*MB.*:\s+\d' }).Count -ne 2) {
            throw "Expected one v5 directory summary in each pane. Controls: $($englishTexts -join ' | ')"
        }
        if ($ExpectMidiFixture) {
            if (-not (Test-Path -LiteralPath (Join-Path (
                    Split-Path -Parent $resolvedExecutable) 'd_CR6139.mid'))) {
                throw 'The startup MIDI fixture was not found beside the test executable.'
            }
            if (($englishTexts | Where-Object {
                $_ -match '^Audio:\s+1.*0\.0 MB.*1 unknown$'
            }).Count -ne 2) {
                throw "The live MIDI directory summaries were unexpected. Controls: $($englishTexts -join ' | ')"
            }
        }

        [void][AudioCommanderGuiNative]::PostMessageW(
            $main.Handle, 0x0111, [IntPtr]104, [IntPtr]::Zero)
        $folderDialog = Wait-ForWindow -ProcessId $app.Id -Class '#32770'
        # The common item dialog may use either the application title or the
        # Windows shell's localized fallback title on different builds.
        if ($folderDialog.Title -notin @('Browse...', 'Open')) {
            throw "The folder picker title was unexpected: $($folderDialog.Title)"
        }
        [void][AudioCommanderGuiNative]::SendMessageW(
            $folderDialog.Handle, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
        $browseDeadline = [DateTime]::UtcNow.AddSeconds(3)
        while (-not [AudioCommanderGuiNative]::IsWindowEnabled($main.Handle) -and
               [DateTime]::UtcNow -lt $browseDeadline) {
            Start-Sleep -Milliseconds 25
        }
        if (-not [AudioCommanderGuiNative]::IsWindowEnabled($main.Handle)) {
            throw 'The main window was not re-enabled after cancelling Browse.'
        }
    }

    for ($theme = 0; $theme -lt $themeNames.Count; ++$theme) {
        Set-AppSettings -MainWindow $main.Handle -ProcessId $app.Id `
            -Theme $theme -Language 0
        if (-not $SkipScreenshots) {
            Save-WindowImage -Window $main.Handle `
                -Path (Join-Path $output ("theme-{0}.png" -f $themeNames[$theme]))
        }
    }

    Set-AppSettings -MainWindow $main.Handle -ProcessId $app.Id `
        -Theme 1 -Language 2
    $italianTexts = Get-ChildTexts -Parent $main.Handle
    foreach ($expected in @('Impostazioni...', 'Ferma', 'Scambia', 'Aggiorna')) {
        if ($italianTexts -notcontains $expected) {
            throw "Italian control not found: $expected. Controls: $($italianTexts -join ' | ')"
        }
    }
    if (-not $SkipScreenshots) {
        Save-WindowImage -Window $main.Handle -Path (Join-Path $output 'italian-light.png')
    }

    [void][AudioCommanderGuiNative]::PostMessageW(
        $main.Handle, 0x0100, [IntPtr]0x70, [IntPtr]::Zero)
    [void][AudioCommanderGuiNative]::PostMessageW(
        $main.Handle, 0x0101, [IntPtr]0x70, [IntPtr]::Zero)
    $help = Wait-ForWindow -ProcessId $app.Id -Class 'AudioCommanderHelp'
    if ($help.Title -ne $ExpectedItalianHelpTitle) {
        throw "Italian F1 Help title was unexpected: $($help.Title)"
    }
    [void][AudioCommanderGuiNative]::SendMessageW(
        $help.Handle, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)

    [void][AudioCommanderGuiNative]::PostMessageW(
        $main.Handle, 0x0100, [IntPtr]0x74, [IntPtr]::Zero)
    [void][AudioCommanderGuiNative]::PostMessageW(
        $main.Handle, 0x0101, [IntPtr]0x74, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 250
    if ($app.HasExited) {
        throw 'Application exited during F5 refresh validation.'
    }

    $second = Start-Process -FilePath $resolvedExecutable `
        -WorkingDirectory (Split-Path -Parent $resolvedExecutable) -PassThru
    $alreadyRunning = Wait-ForWindow -ProcessId $second.Id -Class '#32770'
    if (Get-TopWindows -ProcessId $second.Id |
        Where-Object { $_.Class -eq 'AudioCommanderWindow' }) {
        throw 'The second process created a second main window.'
    }
    [void][AudioCommanderGuiNative]::SendMessageW(
        $alreadyRunning.Handle, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
    if (-not $second.WaitForExit(3000)) {
        throw 'The second process did not exit after its informational dialog closed.'
    }

    [pscustomobject]@{
        ThemesCaptured = if ($SkipScreenshots) { 'skipped' } else { $themeNames.Count }
        EnglishControls = 'passed'
        V5DirectorySummary = if ($ExpectV5Controls) { 'passed' } else { 'not requested' }
        V5BrowseDialog = if ($ExpectV5Controls) { 'passed' } else { 'not requested' }
        ItalianControls = 'passed'
        TransparencyAlpha = if ($OpaqueLayouts) { 'not requested' } else { $startupAlpha }
        F1ItalianHelp = 'passed'
        F5Refresh = 'passed'
        SecondMainWindowBlocked = 'passed'
        ScreenshotDirectory = $output
    }
}
finally {
    if ($second -and -not $second.HasExited) {
        Stop-Process -Id $second.Id -ErrorAction SilentlyContinue
    }
    if ($app -and -not $app.HasExited) {
        $top = Get-TopWindows -ProcessId $app.Id |
            Where-Object { $_.Class -eq 'AudioCommanderWindow' } |
            Select-Object -First 1
        if ($top) {
            [void][AudioCommanderGuiNative]::PostMessageW(
                $top.Handle, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
            [void]$app.WaitForExit(1500)
        }
        if (-not $app.HasExited) {
            Stop-Process -Id $app.Id -ErrorAction SilentlyContinue
        }
    }
    [void][AudioCommanderGuiNative]::WritePrivateProfileStringW(
        'Appearance', 'Opacity', [string]$originalOpacity, $script:SettingsIniPath)
    [void][AudioCommanderGuiNative]::WritePrivateProfileStringW(
        'Appearance', 'Theme', [string]$originalTheme, $script:SettingsIniPath)
    [void][AudioCommanderGuiNative]::WritePrivateProfileStringW(
        'Appearance', 'Language', [string]$originalLanguage, $script:SettingsIniPath)
    if ($originalAcceptance -eq $acceptanceSentinel) {
        [void][AudioCommanderGuiNative]::WritePrivateProfileStringW(
            'Legal', 'Accepted', $null, $script:SettingsIniPath)
    }
    else {
        [void][AudioCommanderGuiNative]::WritePrivateProfileStringW(
            'Legal', 'Accepted', [string]$originalAcceptance, $script:SettingsIniPath)
    }
    if ($originalIniExists) {
        [IO.File]::WriteAllBytes($script:SettingsIniPath, $originalIniBytes)
    }
    elseif (Test-Path -LiteralPath $script:SettingsIniPath) {
        Remove-Item -LiteralPath $script:SettingsIniPath -Force
    }
}
