# Radar Display — ESP32 + LVGL + dump1090

Live aircraft radar pulling data from a local dump1090/readsb instance over WiFi. Two hardware targets supported — swap between them with a single build flag.

| Target | Board | Display | Resolution | Touch |
|---|---|---|---|---|
| **S3** (default) | ESP32-S3 4848S040 | ST7701 RGB | 480×480 | GT911 (I²C) |
| **C6** | ESP32-C6 DevKit | ILI9341 SPI | 240×240 crop | none |

## Quick start

```bash
git clone https://github.com/gregdev/dump1090-radar-display.git radar-display
cd radar-display

# 1. Create your config
cp src/config.example.h src/config.h
# Edit src/config.h: set WiFi SSID/password, dump1090 host IP, and home lat/lon

# 2. Build & flash

# ESP32-S3 + 480×480 RGB (default):
pio run --target upload

# ESP32-C6 + ILI9341 2.4" SPI:
pio run -e esp32-c6-ili9341 --target upload
```

> **Windows users**: see [First-time setup](#first-time-setup) below for PlatformIO details.

## Build targets

The project uses two PlatformIO environments — switching is a one-line change:

```ini
# platformio.ini

[env:esp32-s3-4848s040]   # S3 + ST7701 RGB 480×480 (PSRAM, touch)
[env:esp32-c6-ili9341]     # C6 + ILI9341 SPI  240×240 crop (no PSRAM, no touch)
```

The entire codebase is platform-aware via `-DPLATFORM_C6_ILI9341` — display driver, screen dimensions, touch, and memory allocation all switch automatically. No code changes needed to move between targets.

### C6 wiring (ILI9341)

| ILI9341 pin | ESP32-C6 GPIO |
|---|---|
| SCK | 6 |
| SDI (MOSI) | 7 |
| SDO (MISO) | 2 |
| CS | 18 |
| DC (RS) | 19 |
| RESET | 20 |
| LED (BL) | 21 |

To change pins, edit the `ILI9341_PIN_*` defines under `#ifdef PLATFORM_C6_ILI9341` in `config.h`.

## Hardware

### ESP32-S3 (default)

- **Board**: ESP32-4848S040C_I_Y_3 (Jingcai/Guition)
- **MCU**: ESP32-S3, 16MB Flash, 8MB Octal PSRAM
- **Display**: ST7701 RGB 480×480, 16-bit parallel
- **Touch**: GT911 capacitive (I²C)
- **Library**: [LovyanGFX](https://github.com/lovyan03/LovyanGFX) (display), [LVGL v9](https://lvgl.io/) (UI)

### ESP32-C6 (alternative)

- **Board**: ESP32-C6 DevKitC-1
- **MCU**: ESP32-C6, 8MB Flash, 320KB SRAM (no PSRAM)
- **Display**: ILI9341 2.4" SPI TFT, 240×320 → 240×240 square crop
- **Touch**: none (skipped in firmware)
- **Platform**: [pioarduino/platform-espressif32](https://github.com/pioarduino/platform-espressif32) (Arduino-ESP32 v3.x)

## Project structure

```
platformio.ini          # PlatformIO build config (two envs)
lv_conf.h               # LVGL v9 configuration (PSRAM-aware)
boards/
├── esp32-s3-4848s040.json          # Guition integrated board definition
├── esp32-s3-devkitc-1-n8r8.json    # Bare S3 DevKit, 8MB flash / 8MB PSRAM
├── esp32-s3-devkitc-1-n16r8.json   # Bare S3 DevKit, 16MB flash / 8MB PSRAM
└── esp32-c6-devkitc-1.json         # Custom C6 board (enables Arduino)
flash_win.ps1            # Windows PowerShell flash helper
build_flash.sh           # WSL build/flash helper
src/
├── main.cpp             # Entry point: WiFi → display → LVGL → radar loop
├── lgfx_config.h        # LovyanGFX config (RGB / SPI, platform-aware)
├── radar_ui.h / .cpp    # LVGL canvas-based radar rendering
├── aircraft_data.h / .cpp   # HTTP fetch + JSON parse
├── coord_convert.h / .cpp   # Geo → pixel projection
├── config.h             # WiFi, dump1090 URL, home, display pins & dims (gitignored)
├── config.example.h     # Template — copy to config.h
├── land_mask.h          # Generated land/sea mask data
└── fonts/
    └── lv_font_monoid_12.c  # Custom bitmap font
```

## First-time setup

### Windows

```powershell
git clone <url> radar-display
cd radar-display
python -m pip install platformio
pio run                         # builds S3 firmware
pio run -e esp32-c6-ili9341     # builds C6 firmware
pio run --target upload         # builds + flashes S3
```

### WSL / Linux

```bash
git clone <url> radar-display
cd radar-display
pipx install platformio  # or: python3 -m pip install platformio
pio run                    # build S3
pio run -e esp32-c6-ili9341  # build C6
pio run --target upload    # build + flash S3 (needs USB passthrough on WSL)
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
| **Blank display** | Check pinout in `config.h` matches your wiring; verify rotation setting for C6 |
| **Build fails on C6** | First build downloads the pioarduino platform (~5 min); ensure internet connection |
| **C6: "missing platformio-build-esp32c6.py"** | Ensure `platform =` points to `pioarduino/platform-espressif32` — the stock espressif32 platform lacks C6 Arduino support |
| **No WiFi** | Check SSID/password in `config.h` |
| **No aircraft** | Verify dump1090 is running: open `http://<host>:8080/data/aircraft.json` in browser |
| **Compile error** | Run `./setup.sh` or `.\setup.ps1` to create source symlinks |
| **Upload fails** | In WSL, use `flash_win.ps1` from Windows PowerShell instead |
| **LVGL config warning** | `lv_conf.h` is correctly configured — the warning is cosmetic |
| **Display upside-down or sideways** | Adjust `lcd.setRotation()` value in `main.cpp` (0–3) |
