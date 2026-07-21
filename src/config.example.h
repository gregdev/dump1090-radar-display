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
//  Display & touch — platform-specific
//    Build with -DPLATFORM_C6_ILI9341 for the C6 devkit.
//    Omit the flag to target the ESP32-S3 + 480×480 RGB panel.
// ============================================================
#ifdef PLATFORM_C6_ILI9341
  // ── ESP32-C6 + ILI9341 2.4" SPI (240×320 panel, 240×240 crop) ──
  #define SCR_W               240
  #define SCR_H               240
  #define DISPLAY_OFFSET_X     40    // centre 240-wide canvas on 320-wide panel (rotated)
  #define DISPLAY_OFFSET_Y      0
  #define RADAR_RADIUS_PX     110   // px, circular viewport (half of S3)
  #define RADAR_NO_TOUCH             // no touch digitizer on this setup
  #define SPLASH_TITLE_X      120
  #define SPLASH_TITLE_Y       90
  #define SPLASH_SUBTITLE_X   120
  #define SPLASH_SUBTITLE_Y   130
  #define SPLASH_VERSION_X    120
  #define SPLASH_VERSION_Y    150
  // ILI9341 SPI pinout — adjust to match your wiring
  #define ILI9341_PIN_MOSI     7
  #define ILI9341_PIN_MISO     2
  #define ILI9341_PIN_SCLK     6
  #define ILI9341_PIN_CS      18
  #define ILI9341_PIN_DC      19
  #define ILI9341_PIN_RST     20
  #define ILI9341_PIN_BL      21
  // No touch pins
#else
  // ── ESP32-S3 + ST7701 RGB 480×480 ──
  #define SCR_W               480
  #define SCR_H               480
  #define DISPLAY_OFFSET_X      0
  #define DISPLAY_OFFSET_Y      0
  #define RADAR_RADIUS_PX     220   // px, circular viewport
  #define TOUCH_SDA  19
  #define TOUCH_SCL  45
  #define TOUCH_RST  -1
  #define TOUCH_INT  -1
  #define SPLASH_TITLE_X      240
  #define SPLASH_TITLE_Y      200
  #define SPLASH_SUBTITLE_X   240
  #define SPLASH_SUBTITLE_Y   250
  #define SPLASH_VERSION_X    240
  #define SPLASH_VERSION_Y    280

  // Rotary encoder for brightness dimming
  #define ENCODER_CLK  1
  #define ENCODER_DT   2
  #define ENCODER_SW   -1    // optional push button, -1 = not used
#endif

#define SCR_CX    (SCR_W / 2)
#define SCR_CY    (SCR_H / 2)
#define CIRCLE_R  RADAR_RADIUS_PX
#define LVGL_PARTIAL_BUF_LINES  20
