#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ── Radar UI public API ────────────────────────────────── */

/** Initialise LVGL, create the radar canvas + all UI widgets.
 *  Must be called once after LVGL is initialised. */
void radar_ui_init(void);

/** Register the touch digitizer and create an LVGL input device.
 *  Returns lv_indev_t* (cast to void* to avoid pulling in lvgl.h). */
void * radar_ui_setup_touch(void *lcd_ptr);

/** Update aircraft positions and redraw the radar display.
 *  `aircraft_array` points to an array of `aircraft_t` structs;
 *  `count` is the number of elements. */
void radar_ui_update_aircraft(const void *aircraft_array, int count);

/** Update the system-status bar (WiFi RSSI / IP / feed health). */
void radar_ui_update_status(int wifi_rssi, const char *wifi_ip,
                            bool wifi_connected, int total_aircraft,
                            bool feed_ok);

/** Advance the sweep-line animation by one step. */
void radar_ui_sweep_tick(void);

/** Force a full redraw of the radar canvas. */
void radar_ui_redraw(void);

/** Adjust backlight brightness by `delta` (positive = brighter). */
void radar_ui_adjust_brightness(int delta);

/** Return the current radar range in nautical miles. */
float radar_ui_get_range(void);

/** Feed a raw touch event into the radar UI (tap / swipe handling). */
void radar_ui_handle_input(int16_t x, int16_t y, bool pressed);

#ifdef __cplusplus
}
#endif
