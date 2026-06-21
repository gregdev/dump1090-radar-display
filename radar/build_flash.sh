#!/bin/bash
# ─────────────────────────────────────────────────────────────
#  ESP32-S3 Radar Display — build & flash helpers
#
#  Run from the radar/ directory.
#  On WSL2 USB passthrough is unreliable — flash from Windows
#  PowerShell instead (see flash_win.ps1 below).
# ─────────────────────────────────────────────────────────────
set -euo pipefail
cd "$(dirname "$0")"

# Ensure pipx-installed tools are on PATH
export PATH="$HOME/.local/bin:$PATH"

CMD="${1:-build}"

case "$CMD" in
  build)
    echo "==> Building firmware..."
    pio run
    echo "==> Done.  Flash with:  pio run --target upload"
    ;;

  flash)
    echo "==> Building & flashing..."
    pio run --target upload
    ;;

  monitor)
    echo "==> Serial monitor (Ctrl-] to exit)..."
    pio device monitor --baud 115200
    ;;

  clean)
    echo "==> Cleaning..."
    pio run --target clean
    ;;

  *)
    echo "Usage: $0 {build|flash|monitor|clean}"
    echo ""
    echo "  build     Compile firmware"
    echo "  flash     Build + upload to ESP32-S3"
    echo "  monitor   Open serial monitor"
    echo "  clean     Remove build artifacts"
    echo ""
    echo "If you're on Windows (WSL2 host), flash from PowerShell:"
    echo "  cd \\\\wsl\$\\Ubuntu\\home\\lightbulb\\projects\\radar\\radar"
    echo "  pio run --target upload"
    exit 1
    ;;
esac
