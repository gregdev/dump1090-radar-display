#!/bin/bash
# ─────────────────────────────────────────────────────────────
#  ESP32 Radar Display — setup symlinks for shared sources
#
#  Optional — for IDE code intelligence only.
#  The PlatformIO build works without these via radar/extra_src.py.
#
#  Run once after cloning the repo, from the repository root:
#    ./setup.sh
# ─────────────────────────────────────────────────────────────
set -euo pipefail
cd "$(dirname "$0")"

echo "==> Creating symlinks for shared sources..."

SRC="radar/src"

# Shared source files (from ../src relative to radar/)
for f in aircraft_data.cpp aircraft_data.h coord_convert.cpp coord_convert.h \
         radar_ui.cpp radar_ui.h config.h land_mask.h; do
    ln -sf "../../src/$f" "$SRC/$f"
    echo "  $SRC/$f → ../../src/$f"
done

# Custom font
mkdir -p "$SRC/fonts"
ln -sf "../../src/fonts/lv_font_monoid_12.c" "$SRC/lv_font_monoid_12.c"
echo "  $SRC/lv_font_monoid_12.c → ../../src/fonts/lv_font_monoid_12.c"

echo ""
echo "==> Setup complete."
echo "    Build:  cd radar && pio run"
echo "    Flash:  cd radar && pio run --target upload"
echo ""
echo "    (For WSL users, see radar/flash_win.ps1 for Windows-side flashing)"
