/**
 * ESP32-S3 Radar Display — main entry point.
 *
 * Target:  ESP32-4848S040 (ST7701 RGB 480x480, 8MB PSRAM, 16MB Flash)
 * Display: LovyanGFX
 * Build:   pio run
 * Flash:   pio run --target upload
 */

#include <Arduino.h>
#include <WiFi.h>
#include "lgfx_config.h"

#include "lvgl.h"
#include "config.h"
#include "radar_ui.h"
#include "aircraft_data.h"
#include "coord_convert.h"

/* ── display ────────────────────────────────────────────── */
static LGFX     lcd;
static lv_display_t *disp = nullptr;

/* ── radar data ─────────────────────────────────────────── */
static aircraft_t    aircraft[MAX_AIRCRAFT];
static int           aircraft_count = 0;
static unsigned long last_fetch = 0;
static unsigned long last_sweep = 0;
static bool          fetch_failed = false;
#define FETCH_BACKOFF_MS  10000

/* ── LVGL tick ──────────────────────────────────────────── */
extern "C" uint32_t lv_tick_elapsed_ms(void) {
    return millis();
}

/* ── flush: LVGL draws → push to LGFX ──────────────────── */
static void flush_cb(lv_display_t *d, const lv_area_t *area,
                     uint8_t *px_map) {
    lcd.pushImageDMA(area->x1, area->y1,
                     area->x2 - area->x1 + 1,
                     area->y2 - area->y1 + 1,
                     (lgfx::rgb565_t*)px_map);
    lv_display_flush_ready(d);
}

/* ── display init ───────────────────────────────────────── */
static bool display_init(void) {
    lcd.init();
    lcd.setRotation(0);
    lcd.setColorDepth(16);

    Serial.printf("Display: %dx%d\n", lcd.width(), lcd.height());

    // Diagnostic: fill screen with test pattern to verify display works
    lcd.fillScreen(TFT_RED);
    delay(1000);
    lcd.fillScreen(TFT_GREEN);
    delay(1000);
    lcd.fillScreen(TFT_BLUE);
    delay(1000);
    lcd.fillScreen(TFT_BLACK);

    lcd.setBrightness(255);
    return true;
}

/* ── LVGL init ──────────────────────────────────────────── */
static void lvgl_init(void) {
    lv_init();

    disp = lv_display_create(lcd.width(), lcd.height());
    lv_display_set_flush_cb(disp, flush_cb);

    // Use partial buffer — LGFX handles the DMA via pushImageDMA
    static lv_color_t buf1[480 * 20];
    lv_display_set_buffers(disp, buf1, nullptr, sizeof(buf1),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
}

/* ── WiFi init ──────────────────────────────────────────── */
static void wifi_init(void) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("WiFi connecting");
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 40) {
        delay(500);
        Serial.print(".");
        retries++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
    } else {
        Serial.println("\nWiFi FAILED — will retry in loop");
    }
}

/* ── setup ──────────────────────────────────────────────── */
void setup(void) {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== ESP32 Radar Display ===");
    Serial.printf("dump1090: http://%s:%d%s\n",
                   DUMP1090_HOST, DUMP1090_PORT, DUMP1090_PATH);
    Serial.printf("Home:     %.6f, %.6f\n", HOME_LAT, HOME_LON);
    Serial.printf("Range:    %.0f NM (%d px radius)\n",
                   (double)RADAR_RANGE_NM, RADAR_RADIUS_PX);

    wifi_init();

    if (!display_init()) {
        Serial.println("Display init failed — halting");
        while (1) delay(1000);
    }

    lvgl_init();
    radar_ui_init();
    radar_ui_setup_touch(&lcd);   /* register touch + backlight */

    /* initial data fetch */
    aircraft_count = aircraft_fetch(aircraft, MAX_AIRCRAFT);
    if (aircraft_count < 0) aircraft_count = 0;

    radar_ui_update_aircraft(aircraft, aircraft_count);  /* computes positions */
    last_fetch = millis();
    last_sweep = millis();

    Serial.println("Running.");
}

/* ── loop ───────────────────────────────────────────────── */
void loop(void) {
    unsigned long now = millis();

    if (WiFi.status() != WL_CONNECTED) {
        wifi_init();
    }

    // LVGL housekeeping (no lv_tick_inc — LV_TICK_CUSTOM handles it)
    lv_timer_handler();

    // Data refresh — back off after failures
    unsigned long interval = fetch_failed ? FETCH_BACKOFF_MS : DATA_REFRESH_MS;
    if (now - last_fetch >= interval) {
        int n = aircraft_fetch(aircraft, MAX_AIRCRAFT);
        if (n >= 0) {
            fetch_failed = false;
            aircraft_count = n;
        } else {
            fetch_failed = true;
        }
        bool feed_ok = (n >= 0);
        radar_ui_update_aircraft(aircraft, aircraft_count);  /* computes positions */

        /* update system status in menu */
        radar_ui_update_status(
            WiFi.RSSI(),
            WiFi.localIP().toString().c_str(),
            WiFi.status() == WL_CONNECTED,
            aircraft_count,
            feed_ok);

        last_fetch = now;
    }

    // Sweep animation
    if (now - last_sweep >= SWEEP_UPDATE_MS) {
        radar_ui_sweep_tick();
        radar_ui_update_aircraft(aircraft, aircraft_count);
        last_sweep = now;
    }

    delay(5);
}
