#pragma once

// ============================================================
//  WiFi
// ============================================================
#define WIFI_SSID     "YOUR_SSID"
#define WIFI_PASSWORD "YOUR_PASSWORD"

// ============================================================
//  dump1090 / readsb endpoint
//  Typically: http://<raspberry-pi-ip>:8080/data/aircraft.json
//  Or readsb:  http://<pi-ip>:8042/aircraft.json
// ============================================================
#define DUMP1090_HOST "192.168.68.109"
#define DUMP1090_PORT 8080
#define DUMP1090_PATH "/data/aircraft.json"

// ============================================================
//  Home position — radar centre (decimal degrees)
// ============================================================
#define HOME_LAT  -34.905463157242856
#define HOME_LON  138.635601706212

// ============================================================
//  Radar display settings
// ============================================================
#define RADAR_RADIUS_PX   220       // pixels, circular viewport
#define RADAR_RANGE_NM     40.0f    // nautical miles to edge of circle
#define RADAR_SWEEP_SPEED   4.0f    // degrees per frame (360° ≈ 6 s @ 60 fps)

// Land mask was generated at this range.  When viewing at smaller
// ranges the mask is upscaled.  At larger ranges land is hidden.
// Regenerate land_mask.h if you change this.
#define LAND_MASK_BASE_RANGE_NM  40.0f

// ============================================================
//  Update intervals (milliseconds)
// ============================================================
#define DATA_REFRESH_MS   2000      // fetch new aircraft.json every 2 s
#define SWEEP_UPDATE_MS     16      // ~60 fps sweep animation

// ============================================================
//  Display driver pinout (verify against your board!)
//  Common for "ESP32-S3 4-inch 480×480" boards:
//    ST7796 or ILI9488 on SPI2, FT6336 touch on I2C
// ============================================================
#define TFT_CS    10
#define TFT_DC    11
#define TFT_RST   12
#define TFT_BL     9    // backlight (PWM)
#define TFT_MOSI  13
#define TFT_SCLK  14
#define TFT_MISO   8

#define TOUCH_SDA  4
#define TOUCH_SCL  5
#define TOUCH_RST  6
#define TOUCH_INT  7
