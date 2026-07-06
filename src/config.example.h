#pragma once
/*
 *  config.example.h — copy to config.h and fill in your values.
 */

// ============================================================
//  WiFi
// ============================================================
#define WIFI_SSID     "Your Wifi Name"
#define WIFI_PASSWORD "Your Wifi Password"

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
#define HOME_LAT  -34.000000000000
#define HOME_LON  138.000000000000

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
//  Splash screen
// ============================================================
#define SPLASH_TITLE    "RADAR"
#define SPLASH_TITLE_SIZE    4     // 1=small, 4=large
#define SPLASH_SUBTITLE "dump1090 radar display"
#define SPLASH_SUBTITLE_SIZE 2
#define SPLASH_VERSION  "v1.0"

// ============================================================
//  Display driver pinout
//  This display uses RGB parallel interface (configured in
//  lgfx_config.h).  LCD_RST is the only pin exposed here as
//  a config override — the rest are hardwired in lgfx_config.h.
//  See docs/hardware-reference.md for your board's pinout.
// ============================================================
#define LCD_RST   35    // LCD reset — override for dev-board wiring
