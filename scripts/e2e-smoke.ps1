# SnapOCR end-to-end smoke test (Windows PowerShell 5.1+).
#
# Verifies the minimal workflow on a real desktop:
#   1. app starts and registers hotkeys
#   2. F1 -> drag selection -> confirm -> image copied to clipboard
#   3. F4 -> drag selection -> OCR text copied to clipboard
#   4. Esc -> overlay closes
#
# Usage (from the repository root, after scripts\build.bat):
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\e2e-smoke.ps1
#   powershell ... -File scripts\e2e-smoke.ps1 -BuildDir build-lead
#
# Notes:
# - Requires an interactive desktop session (screen must not be locked).
# - Hotkeys are triggered by posting WM_HOTKEY to the app thread (no keyboard
#   injection, so the active window / IME state does not matter).
# - The selection drag still injects real mouse input; do not use the mouse
#   while the script runs.

param(
    [string]$BuildDir = 'build',
    [int]$DelayAfterHotkeyMs = 2000
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class SnapOcrE2E {
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, uint d, UIntPtr e);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern bool PostThreadMessage(uint tid, uint msg, IntPtr w, IntPtr l);
}
"@
[void][SnapOcrE2E]::SetProcessDPIAware()

# Hotkey ids must match HotkeyManager::Action
$HOTKEY_CAPTURE = 1
$HOTKEY_OCR = 2
$HOTKEY_CANCEL = 3
$HOTKEY_CONFIRM = 4
$WM_HOTKEY = 0x0312

$repoRoot = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repoRoot (Join-Path $BuildDir 'SnapOCR.exe')
if (-not (Test-Path $exe)) {
    Write-Host "[FAIL] $exe not found. Run scripts\build.bat first."
    exit 1
}

$qtBin = $env:QTDIR
if ([string]::IsNullOrEmpty($qtBin)) { $qtBin = 'E:\Qt6\6.11.1\msvc2022_64' }
if (Test-Path (Join-Path $qtBin 'bin')) { $env:PATH = (Join-Path $qtBin 'bin') + ';' + $env:PATH }

$script:failures = 0
function Pass([string]$message) { Write-Host ("[PASS] " + $message) }
function Fail([string]$message) { Write-Host ("[FAIL] " + $message); $script:failures++ }

function Get-AppThreadId([int]$processId) {
    $script:targetPid = $processId
    $script:tid = 0
    $cb = [SnapOcrE2E+EnumProc]{
        param($h, $l)
        $owner = 0
        $threadId = [SnapOcrE2E]::GetWindowThreadProcessId($h, [ref]$owner)
        if ($owner -eq $script:targetPid -and $script:tid -eq 0) { $script:tid = $threadId }
        return $true
    }
    [void][SnapOcrE2E]::EnumWindows($cb, [IntPtr]::Zero)
    return $script:tid
}

function Invoke-Hotkey([int]$threadId, [int]$hotkeyId) {
    [void][SnapOcrE2E]::PostThreadMessage($threadId, $WM_HOTKEY, [IntPtr]$hotkeyId, [IntPtr]::Zero)
}

function Count-Overlays([int]$processId, [string]$titlePart) {
    $script:targetPid = $processId
    $script:count = 0
    $cb = [SnapOcrE2E+EnumProc]{
        param($h, $l)
        $owner = 0
        [void][SnapOcrE2E]::GetWindowThreadProcessId($h, [ref]$owner)
        if ($owner -eq $script:targetPid -and [SnapOcrE2E]::IsWindowVisible($h)) {
            $sb = New-Object System.Text.StringBuilder 200
            [void][SnapOcrE2E]::GetWindowText($h, $sb, 200)
            if ($sb.ToString().Contains($script:titlePart)) { $script:count = $script:count + 1 }
        }
        return $true
    }
    $script:titlePart = $titlePart
    [void][SnapOcrE2E]::EnumWindows($cb, [IntPtr]::Zero)
    return $script:count
}

function Invoke-Drag([int]$x1, [int]$y1, [int]$x2, [int]$y2) {
    [SnapOcrE2E]::SetCursorPos($x1, $y1) | Out-Null
    Start-Sleep -Milliseconds 300
    [SnapOcrE2E]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 200
    for ($step = 1; $step -le 10; $step++) {
        [SnapOcrE2E]::SetCursorPos($x1 + [int](($x2 - $x1) * $step / 10),
                                  $y1 + [int](($y2 - $y1) * $step / 10)) | Out-Null
        Start-Sleep -Milliseconds 60
    }
    [SnapOcrE2E]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 600
}

function Stop-App {
    Get-Process -Name SnapOCR -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 800
}

Stop-App
try { [System.Windows.Forms.Clipboard]::Clear() } catch { }

Write-Host "=== SnapOCR e2e smoke (build dir: $BuildDir) ==="
$app = Start-Process -FilePath $exe -ArgumentList @('--tray') -PassThru
Start-Sleep -Seconds 3
$app.Refresh()
if ($app.HasExited) {
    Write-Host "[FAIL] app exited during startup (exit code $($app.ExitCode))"
    exit 1
}
$threadId = Get-AppThreadId $app.Id
if ($threadId -eq 0) {
    Write-Host "[FAIL] could not resolve app thread id"
    Get-Process -Name SnapOCR -ErrorAction SilentlyContinue | Stop-Process -Force
    exit 1
}
Pass "app started (pid $($app.Id), thread $threadId)"

# ---- 1. capture -> copy ----
Invoke-Hotkey $threadId $HOTKEY_CAPTURE
Start-Sleep -Milliseconds $DelayAfterHotkeyMs
$overlays = Count-Overlays $app.Id 'Overlay'
if ($overlays -ge 1) { Pass "F1 opened the capture overlay" } else { Fail "F1 produced no overlay window" }

Invoke-Drag 400 300 1200 900
Invoke-Hotkey $threadId $HOTKEY_CONFIRM
Start-Sleep -Milliseconds 1500

$hasImage = $false
try { $hasImage = [System.Windows.Forms.Clipboard]::ContainsImage() } catch { }
if ($hasImage) {
    $img = [System.Windows.Forms.Clipboard]::GetImage()
    # The drag goes from physical (400,300) to (1200,900): exactly 801x601 pixels.
    if ($img.Width -eq 801 -and $img.Height -eq 601) {
        Pass "F1 -> selection -> confirm copied image ($($img.Width)x$($img.Height))"
    } else {
        Fail "selection size mismatch: got $($img.Width)x$($img.Height), expected 801x601"
    }
    $img.Dispose()
} else {
    Fail "capture flow produced no clipboard image"
}

# ---- 2. OCR -> text ----
try { [System.Windows.Forms.Clipboard]::Clear() } catch { }
Invoke-Hotkey $threadId $HOTKEY_OCR
Start-Sleep -Milliseconds $DelayAfterHotkeyMs
Invoke-Drag 600 500 1400 1000
Start-Sleep -Milliseconds 3000

$text = ''
try { if ([System.Windows.Forms.Clipboard]::ContainsText()) { $text = [System.Windows.Forms.Clipboard]::GetText() } } catch { }
if ($text.Length -gt 0) {
    Pass "F4 -> selection -> OCR copied $($text.Length) characters"
} else {
    Fail "OCR flow produced no clipboard text"
}

# ---- 3. cancel closes the overlay ----
Invoke-Hotkey $threadId $HOTKEY_CAPTURE
Start-Sleep -Milliseconds $DelayAfterHotkeyMs
Invoke-Hotkey $threadId $HOTKEY_CANCEL
Start-Sleep -Milliseconds 800
if ((Count-Overlays $app.Id 'Overlay') -eq 0) {
    Pass "Esc closed the overlay"
} else {
    Fail "overlay still visible after Esc"
}

# ---- 4. shutdown ----
try { $app.CloseMainWindow() | Out-Null } catch { }
Start-Sleep -Milliseconds 500
Stop-App
if ((Get-Process -Name SnapOCR -ErrorAction SilentlyContinue | Measure-Object).Count -eq 0) {
    Pass "app closed"
} else {
    Fail "app still running after close"
}

if ($script:failures -eq 0) {
    Write-Host "=== result: ALL PASS ==="
    exit 0
}
Write-Host "=== result: $($script:failures) FAILED ==="
exit 1
