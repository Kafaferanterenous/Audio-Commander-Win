param(
    [string]$Executable = (Join-Path $PSScriptRoot '..\build-v5-nmake\audiocommander_v5.exe')
)

$ErrorActionPreference = 'Stop'

Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class AudioCommanderSequenceNative
{
    [DllImport("user32.dll")]
    public static extern IntPtr GetDlgItem(IntPtr window, int controlId);

    [DllImport("user32.dll")]
    public static extern IntPtr SendMessageW(
        IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool PostMessageW(
        IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool IsWindowEnabled(IntPtr window);
}
'@

function Write-SilentWave {
    param(
        [string]$Path,
        [int]$DurationMilliseconds
    )

    $sampleRate = 8000
    $sampleCount = [int]($sampleRate * $DurationMilliseconds / 1000)
    $dataBytes = $sampleCount * 2
    $stream = [IO.File]::Open($Path, [IO.FileMode]::CreateNew)
    $writer = [IO.BinaryWriter]::new($stream)
    try {
        $writer.Write([Text.Encoding]::ASCII.GetBytes('RIFF'))
        $writer.Write([int](36 + $dataBytes))
        $writer.Write([Text.Encoding]::ASCII.GetBytes('WAVEfmt '))
        $writer.Write([int]16)
        $writer.Write([int16]1)
        $writer.Write([int16]1)
        $writer.Write([int]$sampleRate)
        $writer.Write([int]($sampleRate * 2))
        $writer.Write([int16]2)
        $writer.Write([int16]16)
        $writer.Write([Text.Encoding]::ASCII.GetBytes('data'))
        $writer.Write([int]$dataBytes)
        $writer.Write([byte[]]::new($dataBytes))
    }
    finally {
        $writer.Dispose()
        $stream.Dispose()
    }
}

function Invoke-ControlClick {
    param([IntPtr]$Control)

    [void][AudioCommanderSequenceNative]::PostMessageW(
        $Control, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 50
}

function Invoke-MouseClick {
    param(
        [IntPtr]$Control,
        [int]$X,
        [int]$Y
    )

    $position = [IntPtr](($Y -shl 16) -bor ($X -band 0xffff))
    [void][AudioCommanderSequenceNative]::PostMessageW(
        $Control, 0x0201, [IntPtr]1, $position)
    [void][AudioCommanderSequenceNative]::PostMessageW(
        $Control, 0x0202, [IntPtr]::Zero, $position)
    Start-Sleep -Milliseconds 50
}

function Wait-ForStopState {
    param(
        [IntPtr]$StopButton,
        [bool]$Enabled,
        [int]$TimeoutMilliseconds = 3000
    )

    $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMilliseconds)
    do {
        if ([AudioCommanderSequenceNative]::IsWindowEnabled($StopButton) -eq $Enabled) {
            return
        }
        Start-Sleep -Milliseconds 25
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Stop button did not reach enabled=$Enabled."
}

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$sourceDirectory = Split-Path -Parent $resolvedExecutable
$temporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$fixtureDirectory = Join-Path $temporaryRoot (
    'AudioCommander-sequential-' + [Guid]::NewGuid().ToString('N'))
$fixtureFullPath = [IO.Path]::GetFullPath($fixtureDirectory)
if (-not $fixtureFullPath.StartsWith($temporaryRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Refusing to create the sequential fixture outside the temporary directory.'
}

$app = $null
$main = [IntPtr]::Zero
try {
    New-Item -ItemType Directory -Path $fixtureFullPath | Out-Null
    Copy-Item -LiteralPath $resolvedExecutable -Destination $fixtureFullPath
    $runtimeLibraries = @(Get-ChildItem -LiteralPath $sourceDirectory -Filter '*.dll' |
        Where-Object { $_.Name -match '^(avcodec|avformat|avutil|swresample)-\d+\.dll$' })
    if ($runtimeLibraries.Count -ne 4) {
        throw "Expected four FFmpeg runtime libraries beside v5; found $($runtimeLibraries.Count)."
    }
    foreach ($library in $runtimeLibraries) {
        Copy-Item -LiteralPath $library.FullName -Destination $fixtureFullPath
    }
    Write-SilentWave -Path (Join-Path $fixtureFullPath '01-first.wav') -DurationMilliseconds 1000
    [IO.File]::WriteAllText(
        (Join-Path $fixtureFullPath '02-corrupt.wav'), 'not audio',
        [Text.Encoding]::ASCII)
    Write-SilentWave -Path (Join-Path $fixtureFullPath '03-last.wav') -DurationMilliseconds 2000
    [IO.File]::WriteAllLines(
        (Join-Path $fixtureFullPath 'audiocommander.ini'),
        @(
            '[Legal]', 'Accepted=1',
            '[Appearance]', 'Opacity=100', 'Language=0', 'InterfaceSize=100',
            '[Playback]', 'Sequential=1', 'Volume=0'
        ),
        [Text.Encoding]::Unicode)

    $fixtureExecutable = Join-Path $fixtureFullPath (Split-Path -Leaf $resolvedExecutable)
    $app = Start-Process -FilePath $fixtureExecutable -WorkingDirectory $fixtureFullPath -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds(8)
    do {
        Start-Sleep -Milliseconds 50
        $app.Refresh()
        $main = [IntPtr]$app.MainWindowHandle
    } while ($main -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $deadline)
    if ($main -eq [IntPtr]::Zero) {
        throw 'AudioCommander main window was not found.'
    }

    $leftList = [AudioCommanderSequenceNative]::GetDlgItem($main, 103)
    $rightList = [AudioCommanderSequenceNative]::GetDlgItem($main, 203)
    $leftUp = [AudioCommanderSequenceNative]::GetDlgItem($main, 101)
    $stopButton = [AudioCommanderSequenceNative]::GetDlgItem($main, 27)
    if ($leftList -eq [IntPtr]::Zero -or $rightList -eq [IntPtr]::Zero -or
        $leftUp -eq [IntPtr]::Zero -or $stopButton -eq [IntPtr]::Zero) {
        throw 'One or more v5 playback controls were not found.'
    }

    # Name order, left pane: first WAV, corrupt WAV (skipped), final WAV.
    Write-Output 'Phase: Name order / left pane'
    Invoke-MouseClick -Control $leftList -X 80 -Y 38
    Wait-ForStopState -StopButton $stopButton -Enabled $true
    Start-Sleep -Milliseconds 1400
    if (-not [AudioCommanderSequenceNative]::IsWindowEnabled($stopButton)) {
        throw 'Name-order playback stopped instead of skipping the corrupt queued file.'
    }
    Invoke-ControlClick -Control $leftUp
    Start-Sleep -Milliseconds 1400
    if (-not [AudioCommanderSequenceNative]::IsWindowEnabled($stopButton)) {
        throw 'Browsing the left pane cleared its playback snapshot.'
    }
    Wait-ForStopState -StopButton $stopButton -Enabled $false -TimeoutMilliseconds 1800

    # Size order, right pane: corrupt, first WAV, final WAV. Stop must clear the queue.
    Write-Output 'Phase: Size order / right pane / manual Stop'
    $rightHeader = [AudioCommanderSequenceNative]::SendMessageW(
        $rightList, 0x101F, [IntPtr]::Zero, [IntPtr]::Zero)
    Invoke-MouseClick -Control $rightHeader -X 325 -Y 10
    Start-Sleep -Milliseconds 100
    Invoke-MouseClick -Control $rightList -X 80 -Y 58
    Wait-ForStopState -StopButton $stopButton -Enabled $true
    Invoke-ControlClick -Control $stopButton
    Wait-ForStopState -StopButton $stopButton -Enabled $false
    Start-Sleep -Milliseconds 1400
    if ([AudioCommanderSequenceNative]::IsWindowEnabled($stopButton)) {
        throw 'Manual Stop did not clear the Size-order queue.'
    }

    # Duration order, right pane: unknown/corrupt, 1-second WAV, 2-second WAV.
    Write-Output 'Phase: Duration order / right pane'
    Invoke-MouseClick -Control $rightHeader -X 405 -Y 10
    Start-Sleep -Milliseconds 100
    Invoke-MouseClick -Control $rightList -X 80 -Y 58
    Wait-ForStopState -StopButton $stopButton -Enabled $true
    Start-Sleep -Milliseconds 1400
    if (-not [AudioCommanderSequenceNative]::IsWindowEnabled($stopButton)) {
        throw 'Duration-order automatic-next playback did not reach the final WAV.'
    }
    Wait-ForStopState -StopButton $stopButton -Enabled $false -TimeoutMilliseconds 2600

    [pscustomobject]@{
        NameOrderLeft = 'passed'
        CorruptFileSkipped = 'passed'
        BrowseSnapshotRetained = 'passed'
        SizeOrderRight = 'passed'
        ManualStopClearsQueue = 'passed'
        DurationOrderRight = 'passed'
        AutomaticEndStops = 'passed'
    }
}
finally {
    if ($app -and -not $app.HasExited) {
        if ($main -ne [IntPtr]::Zero) {
            [void][AudioCommanderSequenceNative]::PostMessageW(
                $main, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
            [void]$app.WaitForExit(3000)
        }
        if (-not $app.HasExited) {
            Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
            [void]$app.WaitForExit(3000)
        }
    }
    if (Test-Path -LiteralPath $fixtureFullPath) {
        $cleanupPath = [IO.Path]::GetFullPath($fixtureFullPath)
        if (-not $cleanupPath.StartsWith(
                $temporaryRoot, [StringComparison]::OrdinalIgnoreCase) -or
            -not (Split-Path -Leaf $cleanupPath).StartsWith(
                'AudioCommander-sequential-', [StringComparison]::Ordinal)) {
            throw "Refusing unsafe fixture cleanup path: $cleanupPath"
        }
        $cleanupError = $null
        for ($attempt = 0; $attempt -lt 20; ++$attempt) {
            try {
                Remove-Item -LiteralPath $cleanupPath -Recurse -Force -ErrorAction Stop
                $cleanupError = $null
                break
            }
            catch {
                $cleanupError = $_
                Start-Sleep -Milliseconds 100
            }
        }
        if ($cleanupError) {
            throw $cleanupError
        }
    }
}
