/**
 * ESP32 Radar Display — main entry point.
 *
 * Target 1:  ESP32-S3 + ST7701 RGB 480x480 (ESP32-4848S040, 8MB PSRAM)
 * Target 2:  ESP32-C6 + ILI9341 SPI 240x320 → 240x240 crop
 *
 * Select with build flag: -DPLATFORM_C6_ILI9341
 * Display: LovyanGFX
 * Build:   pio run
 * Flash:   pio run --target upload
 */

#include <Arduino.h>
#include <WiFi.h>
#ifndef PLATFORM_C6_ILI9341
#include <soc/lcd_cam_struct.h>
#endif
#include "config.h"
#include "lgfx_config.h"

#include "lvgl.h"
#include "radar_ui.h"
#include "aircraft_data.h"
#include "coord_convert.h"

/* ── display ────────────────────────────────────────────── */
static LGFX     lcd;
static lv_display_t *disp = nullptr;

/* ── touch (global — fed from loop; nullptr on C6) ─────── */
static lv_indev_t *touch_indev = nullptr;

/* ── radar data ─────────────────────────────────────────── */
static aircraft_t    aircraft[MAX_AIRCRAFT];
static int           aircraft_count = 0;
static unsigned long last_fetch = 0;
static unsigned long last_sweep = 0;
static bool          fetch_failed = false;
#define FETCH_BACKOFF_MS  10000

/* ── rotary encoder state machine ───────────────────────── */
#if defined(ENCODER_CLK) && defined(ENCODER_DT)
static int8_t  enc_state   = 0;
static int16_t enc_counter = 0;
static const int8_t enc_table[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

static int8_t encoder_read(void) {
    int clk = digitalRead(ENCODER_CLK);
    int dt  = digitalRead(ENCODER_DT);
    enc_state = (int8_t)(((enc_state << 2) | (clk << 1) | dt) & 0x0F);
    return enc_table[enc_state];
}
#endif

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
    lcd.setAddrWindow(area->x1 + DISPLAY_OFFSET_X, area->y1 + DISPLAY_OFFSET_Y, w, h);
    lcd.pushPixelsDMA((uint16_t*)px_map, w * h, true);
    lcd.endWrite();

    lv_display_flush_ready(d);
}

/* ── display init ───────────────────────────────────────── */
static bool display_init(void) {
    Serial.println("Display: calling lcd.init()...");
    if (!lcd.init()) {
        Serial.println("Display: lcd.init() FAILED!");
        return false;
    }
    Serial.println("Display: lcd.init() OK");
    Serial.flush();

#ifndef PLATFORM_C6_ILI9341
    // Re-enable VSYNC interrupt now that init_impl is complete
    Serial.println("Display: re-enabling vsync...");
    LCD_CAM.lc_dma_int_ena.val = 1;
    Serial.println("Display: vsync alive");
#endif

    lcd.setRotation(0);
    lcd.setColorDepth(16);
    lcd.setBrightness(255);

    Serial.printf("Display: %dx%d\n", lcd.width(), lcd.height());

    // Skip fillScreen/splash — framebuffer ops on PSRAM may watchdog.
    // LVGL radar UI will fill the screen on first render.
    Serial.println("Display: init complete, handing off to LVGL");

    // brief pause so the user sees it before WiFi init
    delay(800);

    return true;
}

/* ── LVGL init ──────────────────────────────────────────── */
static void lvgl_init(void) {
    lv_init();

    disp = lv_display_create(SCR_W, SCR_H);
    lv_display_set_flush_cb(disp, flush_cb);

    // Use partial buffer — LGFX handles the DMA via pushImageDMA
    static lv_color_t buf1[SCR_W * LVGL_PARTIAL_BUF_LINES];
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
    // Disable task watchdogs so we can prove/disprove the
    // "PSRAM write is just slow due to GDMA bus contention" theory.
    disableCore0WDT();
    disableCore1WDT();

    Serial.begin(115200);
    delay(1000);

    /* I2C scan: GT911 found at 0x14 and 0x5D (confirmed). */

    Serial.println("\n=== ESP32 Radar Display ===");
    Serial.printf("dump1090: http://%s:%d%s\n",
                   DUMP1090_HOST, DUMP1090_PORT, DUMP1090_PATH);
    Serial.printf("Home:     %.6f, %.6f\n", HOME_LAT, HOME_LON);
    Serial.printf("Range:    %.0f NM (%d px radius)\n",
                   (double)RADAR_RANGE_NM, RADAR_RADIUS_PX);

#if defined(ENCODER_CLK) && defined(ENCODER_DT)
    pinMode(ENCODER_CLK, INPUT_PULLUP);
    pinMode(ENCODER_DT,  INPUT_PULLUP);
    Serial.printf("Encoder: CLK=GPIO%d DT=GPIO%d\n", ENCODER_CLK, ENCODER_DT);
#endif

    /* Quick PSRAM test — small writes, check speed */
    {
        Serial.printf("PSRAM: free=%u  DRAM: free=%u  low=%u\n",
            heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
            heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
            heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));

        if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > 0) {
            size_t test_sz = 4096;
            uint8_t* p = (uint8_t*)heap_caps_malloc(test_sz, MALLOC_CAP_SPIRAM);
            Serial.printf("PSRAM: alloc 4KB -> %p\n", p);
            if (p) {
                uint32_t t0 = micros();
                memset(p, 0x55, test_sz);
                uint32_t t1 = micros();
                Serial.printf("PSRAM: memset 4KB in %u us\n", t1-t0);
                bool ok = (p[0]==0x55 && p[test_sz-1]==0x55);
                Serial.printf("PSRAM: verify %s\n", ok ? "PASS" : "FAIL");
                heap_caps_free(p);
                Serial.printf("PSRAM: after free: %u\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
            }
        } else {
            Serial.println("PSRAM: NOT AVAILABLE");
        }
    }

    // Draw the radar UI's static canvas (rings/crosshair/land mask) into
    // its PSRAM buffer BEFORE the RGB panel is initialized. The panel's
    // continuous GDMA refresh (also PSRAM-resident) starts as soon as
    // lcd.init() completes, and large sequential CPU writes to PSRAM
    // while that GDMA is active reliably trip a watchdog reset. Doing
    // the one-time expensive full-buffer draw first, with no GDMA
    // running yet, sidesteps that entirely.
    lvgl_init();
    Serial.printf("HEAP: before radar_ui_init free=%u\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    radar_ui_init();

    Serial.printf("HEAP: before display_init free=%u\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    if (!display_init()) {
        Serial.println("Display init failed — halting");
        while (1) delay(1000);
    }

    Serial.printf("HEAP: after display_init free=%u\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    // TEMP: skip WiFi to isolate radar UI crash
    // wifi_init();
    Serial.println("WiFi SKIPPED (debug)");

#ifdef RADAR_NO_TOUCH
    touch_indev = nullptr;
    (void)lcd;  // unused on C6 (backlight set via LGFX directly)
#else
    touch_indev = (lv_indev_t*)radar_ui_setup_touch(&lcd);  /* register touch + backlight */
#endif

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
#ifndef RADAR_NO_TOUCH
    if (touch_indev) lv_indev_read(touch_indev);
#endif

    // LVGL housekeeping — every ~5 ms
    {
        static unsigned long last_lvgl = 0;
        if (now - last_lvgl >= 5) {
            last_lvgl = now;
            lv_timer_handler();
        }
    }

    // Rotary encoder — poll every loop, apply brightness change every 4 detents
#if defined(ENCODER_CLK) && defined(ENCODER_DT)
    {
        int8_t dir = encoder_read();
        if (dir != 0) {
            enc_counter += dir;
            if (enc_counter >= 4 || enc_counter <= -4) {
                int delta = (enc_counter > 0) ? 3 : -3;
                enc_counter = 0;
                radar_ui_adjust_brightness(delta);
            }
        }
    }
#endif

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

    // Sweep animation removed: it drove a full-canvas 460KB PSRAM
    // redraw at ~60 fps (radar_ui_redraw() -> radar_draw_static()),
    // which collided with the RGB panel's continuous GDMA refresh
    // (also PSRAM-resident) and caused watchdog resets. The sweep
    // line was rarely visible anyway (show_sweep is normally false).
    (void)last_sweep;

    delay(1);  // yield
}
