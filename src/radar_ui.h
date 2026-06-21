#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * Initialise the radar UI on the active LVGL display.
 * Call once after LVGL + display driver are set up.
 */
void radar_ui_init(void);

/**
 * Update aircraft positions on the canvas.
 * `aircraft` array of `count` entries; each has screen_x/y pre-computed.
 * Call every DATA_REFRESH_MS.
 */
void radar_ui_update_aircraft(const void *aircraft_array, int count);

/**
 * Advance the sweep-line animation.
 * Call every SWEEP_UPDATE_MS from a timer or the main loop.
 */
void radar_ui_sweep_tick(void);

/**
 * Feed raw touch/pointer coordinates to the radar UI for
 * tap-to-inspect and swipe-menu handling.
 *
 * @param x       Screen X coordinate (0–479)
 * @param y       Screen Y coordinate (0–479)
 * @param pressed true = finger down, false = finger up
 */
void radar_ui_handle_input(int16_t x, int16_t y, bool pressed);

/**
 * Update system status for the hidden status/menu drawer.
 * Call periodically (e.g. every 2 seconds).
 *
 * @param wifi_rssi      WiFi RSSI in dBm (0 if unknown)
 * @param wifi_ip         IP address string ("--" if disconnected)
 * @param wifi_connected  true if WiFi is connected
 * @param total_aircraft  Total aircraft count from dump1090
 * @param feed_ok         true if last fetch succeeded
 */
void radar_ui_update_status(int wifi_rssi, const char *wifi_ip,
                            bool wifi_connected, int total_aircraft,
                            bool feed_ok);

/**
 * Set up the LVGL touch input device using the ESP_Panel touch driver.
 * Call once after display_init() and lvgl_init().
 *
 * @param panel  Pointer to the initialised ESP_Panel instance.
 */
void radar_ui_setup_touch(void *panel);

/**
 * Get the current radar range in nautical miles (may differ from
 * RADAR_RANGE_NM if the user changed it via the swipe menu).
 */
float radar_ui_get_range(void);

#ifdef __cplusplus
}
#endif
