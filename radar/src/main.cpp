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
#include <esp_task_wdt.h>
#include <soc/timer_group_struct.h>
#include <soc/lcd_cam_struct.h>
#include "config.h"
#include "lgfx_config.h"

#include "lvgl.h"
#include "radar_ui.h"
#include "aircraft_data.h"
#include "coord_convert.h"

/* ── display ────────────────────────────────────────────── */
static LGFX     lcd;
static lv_display_t *disp = nullptr;

/* ── touch (global — fed from loop) ─────────────────────── */
static lv_indev_t *touch_indev = nullptr;

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
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    lcd.startWrite();
    lcd.setAddrWindow(area->x1, area->y1, w, h);
    lcd.pushPixelsDMA((uint16_t*)px_map, w * h, true);
    lcd.endWrite();

    lv_display_flush_ready(d);
}

/* ── display init ───────────────────────────────────────── */
static bool display_init(void) {
    // ── Pre-init diagnostics ──────────────────────────────
    Serial.printf("PSRAM size: %u bytes\n", ESP.getPsramSize());
    Serial.printf("Free PSRAM: %u bytes\n", ESP.getFreePsram());
    Serial.printf("Free heap:  %u bytes\n", ESP.getFreeHeap());

    // Quick PSRAM allocation test
    void *test = heap_caps_malloc(1024, MALLOC_CAP_SPIRAM);
    Serial.printf("PSRAM test: %s\n", test ? "OK" : "FAILED");
    if (test) heap_caps_free(test);

    Serial.flush();  // ensure all output before potentially hanging call

    // ── Large PSRAM test (same size Bus_RGB allocates) ────
    Serial.print("460KB PSRAM test: ");
    Serial.flush();
    void *big = heap_caps_malloc(460800, MALLOC_CAP_SPIRAM);
    if (big) {
        memset(big, 0, 460800);
        Serial.println("OK");
        heap_caps_free(big);
    } else {
        Serial.println("FAILED (null)");
    }
    Serial.flush();

    // ── Test ST7701 SPI pins (47=MOSI, 48=SCLK, 39=CS) ────
    Serial.print("GPIO 47 test: ");
    Serial.flush();
    pinMode(47, OUTPUT);
    digitalWrite(47, LOW);
    Serial.println("OK");
    Serial.print("GPIO 48 test: ");
    Serial.flush();
    pinMode(48, OUTPUT);
    digitalWrite(48, LOW);
    Serial.println("OK");
    Serial.print("GPIO 39 test: ");
    Serial.flush();
    pinMode(39, OUTPUT);
    digitalWrite(39, LOW);
    Serial.println("OK");
    Serial.flush();

    // Disable ALL watchdogs during lcd.init()
    // TG1WDT (interrupt WDT) — direct register disable
    TIMERG1.wdtconfig0.wdt_en = 0;
    TIMERG1.wdtconfig0.wdt_flashboot_mod_en = 0;
    // TG0/Task WDT
    disableLoopWDT();
    esp_task_wdt_deinit();

    Serial.println("Display: calling lcd.init()...");
    Serial.flush();
    bool ok = lcd.init();

    // Re-enable VSYNC interrupt (was paused during ST7701 SPI init)
    LCD_CAM.lc_dma_int_ena.val = 1;

    if (!ok) {
        Serial.println("Display: lcd.init() FAILED!");
        return false;
    }
    Serial.println("Display: lcd.init() OK");
    Serial.flush();

    Serial.println("Display: setRotation...");
    Serial.flush();
    lcd.setRotation(0);
    Serial.println("Display: setColorDepth...");
    Serial.flush();
    lcd.setColorDepth(16);
    Serial.println("Display: setBrightness...");
    Serial.flush();
    lcd.setBrightness(255);

    Serial.printf("Display: %dx%d\n", lcd.width(), lcd.height());
    Serial.flush();

    Serial.println("Display: fillScreen(RED)...");
    Serial.flush();
    lcd.fillScreen(TFT_RED);
    delay(2000);  // let user see red screen
    lcd.fillScreen(TFT_BLACK);

    // Splash screen
    lcd.setTextColor(TFT_GREEN, TFT_BLACK);
    lcd.setTextSize(SPLASH_TITLE_SIZE);
    lcd.setTextDatum(MC_DATUM);
    lcd.drawString(SPLASH_TITLE, 240, 200);
    lcd.setTextColor(0x07E0, TFT_BLACK);  // dim green
    lcd.setTextSize(SPLASH_SUBTITLE_SIZE);
    lcd.drawString(SPLASH_SUBTITLE, 240, 250);
    lcd.setTextColor(TFT_DARKGREEN, TFT_BLACK);
    lcd.drawString(SPLASH_VERSION, 240, 280);

    // brief pause so the user sees it before WiFi init
    delay(800);

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
        yield();  // feed Arduino WDT
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

    /* I2C scan: GT911 found at 0x14 and 0x5D (confirmed). */

    Serial.println("\n=== ESP32 Radar Display ===");
    Serial.printf("dump1090: http://%s:%d%s\n",
                   DUMP1090_HOST, DUMP1090_PORT, DUMP1090_PATH);
    Serial.printf("Home:     %.6f, %.6f\n", HOME_LAT, HOME_LON);
    Serial.printf("Range:    %.0f NM (%d px radius)\n",
                   (double)RADAR_RANGE_NM, RADAR_RADIUS_PX);

    if (!display_init()) {
        Serial.println("Display init failed — halting");
        while (1) delay(1000);
    }

    wifi_init();

    // Re-enable task watchdog now that blocking init is done
    esp_task_wdt_init(10, true);
    enableLoopWDT();

    lvgl_init();
    radar_ui_init();
    touch_indev = (lv_indev_t*)radar_ui_setup_touch(&lcd);  /* register touch + backlight */

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

    // Touch + LVGL — poll every loop (TAMCTec library, no throttling)
    if (touch_indev) lv_indev_read(touch_indev);

    // LVGL housekeeping — every ~5 ms
    {
        static unsigned long last_lvgl = 0;
        if (now - last_lvgl >= 5) {
            last_lvgl = now;
            lv_timer_handler();
        }
    }

    if (WiFi.status() != WL_CONNECTED) {
        wifi_init();
    }

    // Data refresh — back off after failures
    unsigned long interval = fetch_failed ? FETCH_BACKOFF_MS : DATA_REFRESH_MS;
    if (now - last_fetch >= interval) {
        int n = aircraft_fetch(aircraft, MAX_AIRCRAFT);
        Serial.printf("Fetch: %d aircraft\n", n);
        if (n >= 0) {
            fetch_failed = false;
            aircraft_count = n;
        } else {
            fetch_failed = true;
        }
        bool feed_ok = (n >= 0);
        radar_ui_update_aircraft(aircraft, aircraft_count);  /* computes positions + redraws */

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
        radar_ui_redraw();
        last_sweep = now;
    }

    delay(1);  // yield
}
