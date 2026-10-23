# SnapOCR end-to-end smoke test (Windows PowerShell 5.1+).
#
# Verifies the basic workflow on a real desktop:
#   1. app starts and registers F1 / F4 global hotkeys
#   2. F1 -> drag selection -> Enter copies a PNG image to the clipboard
#   3. F4 -> drag selection -> OCR result is copied to the clipboard as text
#
# Usage (from the repository root, after scripts\build.bat):
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\e2e-smoke.ps1
#   powershell ... -File scripts\e2e-smoke.ps1 -BuildDir build-lead
#
# Notes:
# - Requires an interactive desktop session (screen must not be locked).
# - Injects real mouse/keyboard input via user32; do not run while doing other work.

param(
    [string]$BuildDir = 'build',
    [int]$DelayAfterHotkeyMs = 2000
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class SnapOcrE2E {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, uint d, UIntPtr e);
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint f, UIntPtr e);
}
"@
[void][SnapOcrE2E]::SetProcessDPIAware()

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

function Invoke-Key([byte]$virtualKey) {
    [SnapOcrE2E]::keybd_event($virtualKey, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 60
    [SnapOcrE2E]::keybd_event($virtualKey, 0, 2, [UIntPtr]::Zero)
}

function Stop-App {
    Get-Process -Name SnapOCR -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500
}

function Start-App([string[]]$arguments) {
    return Start-Process -FilePath $exe -ArgumentList $arguments -PassThru
}

Stop-App
try { [System.Windows.Forms.Clipboard]::Clear() } catch { }

Write-Host "=== SnapOCR e2e smoke (build dir: $BuildDir) ==="
$app = Start-App @('--tray')
Start-Sleep -Seconds 3
$app.Refresh()
if ($app.HasExited) {
    Write-Host "[FAIL] app exited during startup (exit code $($app.ExitCode))"
    exit 1
}
Pass "app started (pid $($app.Id))"

# ---- 1. capture -> copy ----
Invoke-Key 0x70   # F1
Start-Sleep -Milliseconds $DelayAfterHotkeyMs
Invoke-Drag 400 300 1200 900
Invoke-Key 0x0D   # Enter
Start-Sleep -Milliseconds 1500

$hasImage = $false
try { $hasImage = [System.Windows.Forms.Clipboard]::ContainsImage() } catch { }
if ($hasImage) {
    $img = [System.Windows.Forms.Clipboard]::GetImage()
    if ($img.Width -ge 700 -and $img.Height -ge 500) {
        Pass "F1 -> selection -> Enter copied image ($($img.Width)x$($img.Height))"
    } else {
        Fail "clipboard image too small: $($img.Width)x$($img.Height)"
    }
    $img.Dispose()
} else {
    Fail "F1 flow produced no clipboard image"
}

# ---- 2. quick save (Ctrl+S) ----
$pictures = [Environment]::GetFolderPath('MyPictures')
$saveDir = Join-Path $pictures 'SnapOCR'
$before = @()
if (Test-Path $saveDir) { $before = Get-ChildItem $saveDir -File | ForEach-Object { $_.FullName } }

Invoke-Key 0x70
Start-Sleep -Milliseconds $DelayAfterHotkeyMs
Invoke-Drag 400 300 900 700
[SnapOcrE2E]::keybd_event(0x11, 0, 0, [UIntPtr]::Zero)   # Ctrl down
Start-Sleep -Milliseconds 80
Invoke-Key 0x53                                          # Ctrl+S
[SnapOcrE2E]::keybd_event(0x11, 0, 2, [UIntPtr]::Zero)   # Ctrl up
Start-Sleep -Milliseconds 1500

$after = @()
if (Test-Path $saveDir) { $after = Get-ChildItem $saveDir -File | ForEach-Object { $_.FullName } }
$newFiles = @($after | Where-Object { $before -notcontains $_ })
if ($newFiles.Count -gt 0) {
    Pass "Ctrl+S saved file: $(Split-Path -Leaf $newFiles[0])"
} else {
    Fail "Ctrl+S produced no new file under $saveDir"
}

# ---- 3. screen OCR -> auto copy ----
try { [System.Windows.Forms.Clipboard]::Clear() } catch { }
Invoke-Key 0x73   # F4
Start-Sleep -Milliseconds $DelayAfterHotkeyMs
Invoke-Drag 880 250 1600 860
Start-Sleep -Milliseconds 4000

$text = ''
try { $text = [System.Windows.Forms.Clipboard]::GetText() } catch { }
if ($text.Length -eq 0) {
    # OCR can be flaky under focus changes; retry once.
    try { [System.Windows.Forms.Clipboard]::Clear() } catch { }
    Invoke-Key 0x73
    Start-Sleep -Milliseconds $DelayAfterHotkeyMs
    Invoke-Drag 880 250 1600 860
    Start-Sleep -Milliseconds 4000
    try { $text = [System.Windows.Forms.Clipboard]::GetText() } catch { }
}
if ($text.Length -gt 0) {
    Pass "F4 -> selection -> OCR copied $($text.Length) characters"
} else {
    Fail "F4 OCR produced no clipboard text"
}

$app.Refresh()
if (-not $app.HasExited) {
    $app.Kill()
    Pass "app closed"
}
Stop-App

Write-Host "=== result: $(if ($script:failures -eq 0) { 'ALL PASS' } else { "$script:failures FAILED" }) ==="
if ($script:failures -ne 0) { exit 1 }
exit 0
