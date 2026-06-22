# Radar Display — ESP32-S3 + LVGL + dump1090

Live aircraft radar on a 4" 480×480 smart display (ESP32-4848S040), pulling data from a local dump1090/readsb instance over WiFi.

## Quick start

```bash
git clone https://github.com/gregdev/dump1090-radar-display.git radar-display
cd radar-display

# 1. Create your config
cp src/config.example.h src/config.h
# Edit src/config.h: set WiFi SSID/password, dump1090 host IP, and home lat/lon

# 2. Build & flash (ESP32)
cd radar
pio run --target upload

# Or run the PC simulator (no hardware needed)
cd pc_sim && ./run.sh        # Linux/WSL
# Windows: cmake -B build && cmake --build build && .\build\Debug\radar_sim.exe
```

> **Windows users**: see [First-time setup](#first-time-setup) below for PlatformIO and symlink details.

## Hardware

- **Board**: ESP32-4848S040C_I_Y_3 (Jingcai/Guition)
- **MCU**: ESP32-S3, 16MB Flash, 8MB Octal PSRAM
- **Display**: ST7701 RGB 480×480, 16-bit parallel
- **Touch**: GT911 capacitive (I2C)
- **Library**: [LovyanGFX](https://github.com/lovyan03/LovyanGFX) (display), [LVGL v9](https://lvgl.io/) (UI)

## Project structure

```
radar/
├── platformio.ini          # PlatformIO build config
├── lv_conf.h               # LVGL v9 configuration
├── setup.sh / setup.ps1    # Optional: symlinks for IDE code intelligence
├── flash_win.ps1           # Windows PowerShell flash helper
├── build_flash.sh          # WSL build/flash helper
├── src/
│   ├── main.cpp            # Entry point: WiFi → display → LVGL → radar loop
│   ├── lgfx_config.h       # LovyanGFX pinout & panel config
│   ├── radar_ui.h / .cpp   # LVGL canvas-based radar rendering
│   ├── aircraft_data.h / .cpp  # HTTP fetch + JSON parse
│   ├── coord_convert.h / .cpp  # Geo → pixel projection
│   └── config.h            # WiFi, dump1090 URL, home position
├── fonts/
│   └── lv_font_monoid_12.c # Custom bitmap font
└── pc_sim/                 # PC simulator (data layer only)
```

## First-time setup

> **The Quick Start above is all you need.** The `setup.sh` / `setup.ps1` scripts below are optional — they create symlinks for IDE code intelligence (Go to Definition, etc.). The PlatformIO build uses `radar/extra_src.py` to find shared sources automatically.

### Windows

```powershell
git clone <url> radar
cd radar
python -m pip install platformio
.\setup.ps1           # optional: symlinks for IDE support
cd radar
pio run               # builds firmware
pio run --target upload  # builds + flashes
```

### WSL / Linux

```bash
git clone <url> radar
cd radar
pipx install platformio  # or: python3 -m pip install platformio
./setup.sh                # optional: symlinks for IDE support
cd radar
pio run                    # build
pio run --target upload    # build + flash (needs USB passthrough on WSL)
```

> **WSL notes**: USB passthrough is unreliable. Use `flash_win.ps1` from Windows PowerShell instead, or attach the device via `usbipd` (`usbipd bind --busid X-Y`, `usbipd attach --wsl --busid X-Y`).

## Configuration

Edit `src/config.h`:

```c
#define WIFI_SSID     "MyWiFi"
#define WIFI_PASSWORD "password123"
#define DUMP1090_HOST "192.168.1.100"   // your Pi/PC running dump1090
#define DUMP1090_PORT 8080
#define HOME_LAT      51.5074           // your latitude
#define HOME_LON      -0.1278           // your longitude
#define RADAR_RANGE_NM 40               // radar range in nautical miles
```

## Expected serial output

```
=== ESP32 Radar Display ===
dump1090: http://192.168.1.100:8080/data/aircraft.json
Home:     51.507400, -0.127800
Range:    40 NM (220 px radius)
WiFi connecting..........
WiFi connected: 192.168.1.42
Display: 480x480
Touch: LVGL indev registered
Initial fetch: 12 aircraft
Running.
```

## dump1090 API

Fetches `http://<host>:<port>/data/aircraft.json` every 2 seconds. Expected format:

```json
[{
  "hex": "4ca89b",
  "flight": "RYR123  ",
  "lat": 51.52, "lon": -0.10,
  "alt_geom": 35000, "gs": 440,
  "track": 270, "vert_rate": 0, "seen": 0.3
}]
```

Aircraft without `lat`/`lon` are skipped automatically.

## Land mask

To generate the land mask background for your location:

1. Open [`tools/land_mask_map.html`](tools/land_mask_map.html) in a browser
2. Enter your home lat/lon and range → **Update Map**
3. **Download land_mask.h** → drop it into `src/`

## Troubleshooting

| Issue | Fix |
|-------|-----|
| **Blank display** | Verify ESP32-4848S040 — other boards need different LGFX config in `lgfx_config.h` |
| **No WiFi** | Check SSID/password in `config.h` |
| **No aircraft** | Verify dump1090 is running: open `http://<host>:8080/data/aircraft.json` in browser |
| **Compile error** | Run `./setup.sh` or `.\setup.ps1` to create source symlinks |
| **Upload fails** | In WSL, use `flash_win.ps1` from Windows PowerShell instead |
| **LVGL config warning** | `lv_conf.h` is correctly configured — the warning is cosmetic |
