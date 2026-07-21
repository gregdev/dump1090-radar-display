# ─────────────────────────────────────────────────────────────
#  ESP32-S3 Radar Display — Windows flash helper
#
#  Run from Windows PowerShell (not WSL).
#  Prerequisites: PlatformIO Core installed on Windows
#    python -m pip install platformio esptool
#
#  Usage:
#    .\flash_win.ps1 build          Compile firmware
#    .\flash_win.ps1 flash          Build + upload to ESP32-S3
#    .\flash_win.ps1 monitor        Serial monitor (Ctrl-] to exit)
#    .\flash_win.ps1 clean          Remove build artifacts
# ─────────────────────────────────────────────────────────────
param (
    [ValidateSet("build", "flash", "monitor", "clean")]
    [string]$Command = "flash"
)

$ErrorActionPreference = "Stop"

# ── Paths ───────────────────────────────────────────────────
# Use the directory this script lives in (works regardless of WSL distro name)
$ProjectDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ProjectDir
Write-Host "Project dir: $ProjectDir" -ForegroundColor Cyan

# ── Check PlatformIO ────────────────────────────────────────
$pio = Get-Command pio -ErrorAction SilentlyContinue
if (-not $pio) {
    Write-Host "PlatformIO not found. Install with: python -m pip install platformio" -ForegroundColor Red
    exit 1
}

# ── Detect COM port ─────────────────────────────────────────
function Get-ESP32COMPort {
    # ESP32-S3 typically shows up as Silicon Labs CP210x or similar
    $com = Get-CimInstance Win32_PnPEntity |
           Where-Object { $_.Name -match "Silicon|CP210|CH340|CH343|USB.*Serial|COM\d+" } |
           ForEach-Object { if ($_.Name -match 'COM(\d+)') { "COM$($Matches[1])" } } |
           Select-Object -First 1
    return $com
}

# ── Commands ────────────────────────────────────────────────
switch ($Command) {
    "build" {
        Write-Host "==> Building firmware..." -ForegroundColor Green
        pio run
        Write-Host "==> Done. Flash with: .\flash_win.ps1 flash" -ForegroundColor Green
    }

    "flash" {
        $comPort = Get-ESP32COMPort
        if ($comPort) {
            Write-Host "Detected port: $comPort" -ForegroundColor Cyan
        } else {
            Write-Host "WARNING: Could not auto-detect COM port." -ForegroundColor Yellow
            Write-Host "PlatformIO will try to detect it. If it fails, check Device Manager." -ForegroundColor Yellow
        }

        Write-Host "==> Building & flashing..." -ForegroundColor Green
        pio run --target upload

        if ($LASTEXITCODE -eq 0) {
            Write-Host "==> Flash complete!" -ForegroundColor Green
        } else {
            Write-Host @"
==> Flash failed. Troubleshooting:" -ForegroundColor Red
1. Is the ESP32-S3 plugged in and in download mode? (Hold BOOT, tap RST, release BOOT)
2. Check Device Manager → Ports (COM & LPT) for the correct port
3. Try manually: pio run --target upload --upload-port COM3
"@
        }
    }

    "monitor" {
        Write-Host "==> Serial monitor (Ctrl-] to exit)..." -ForegroundColor Green
        $comPort = Get-ESP32COMPort
        if ($comPort) {
            pio device monitor --baud 115200 --port $comPort
        } else {
            pio device monitor --baud 115200
        }
    }

    "clean" {
        Write-Host "==> Cleaning..." -ForegroundColor Green
        pio run --target clean
        Write-Host "==> Done." -ForegroundColor Green
    }
}
