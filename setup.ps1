# Setup script for Windows
# Creates symlinks for shared sources. Run once after cloning.
# Requires Developer Mode (Settings > Update & Security > For Developers)
# or run PowerShell as Administrator.

param(
    [switch]$Copy  # Use copy instead of symlinks (if Developer Mode is off)
)

$ErrorActionPreference = "Stop"
Set-Location (Split-Path $MyInvocation.MyCommand.Path)

$srcDir = "radar\src"
$sharedDir = "src"

Write-Host "==> Setting up shared source links..." -ForegroundColor Green

$files = @(
    "aircraft_data.cpp", "aircraft_data.h",
    "coord_convert.cpp", "coord_convert.h",
    "radar_ui.cpp", "radar_ui.h",
    "config.h", "land_mask.h"
)

foreach ($f in $files) {
    $target = "..\..\$sharedDir\$f"
    $link   = "$srcDir\$f"

    if (Test-Path $link) { Remove-Item $link -Force }

    if ($Copy) {
        Copy-Item $target $link
        Write-Host "  copied: $link"
    } else {
        New-Item -ItemType SymbolicLink -Path $link -Target $target -Force | Out-Null
        Write-Host "  linked: $link -> $target"
    }
}

# Font
$fontTarget = "..\..\$sharedDir\fonts\lv_font_monoid_12.c"
$fontLink   = "$srcDir\lv_font_monoid_12.c"
if (Test-Path $fontLink) { Remove-Item $fontLink -Force }
if ($Copy) {
    Copy-Item $fontTarget $fontLink
} else {
    New-Item -ItemType SymbolicLink -Path $fontLink -Target $fontTarget -Force | Out-Null
}
Write-Host "  linked: $fontLink -> $fontTarget"

Write-Host ""
Write-Host "==> Setup complete." -ForegroundColor Green
Write-Host "    Build:  cd radar && pio run"
Write-Host "    Flash:  cd radar && pio run --target upload"
Write-Host ""
Write-Host "    If symlinks failed, retry as Administrator or use: .\setup.ps1 -Copy"
