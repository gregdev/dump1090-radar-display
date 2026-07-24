#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * Initialise the MQTT client and connect to the broker.
 * Publishes Home Assistant auto-discovery messages on first connect.
 * Must be called after WiFi is connected.
 */
void mqtt_init(void);

/**
 * Signal that setup() is complete and the MQTT task may begin.
 * Call once at the very end of setup().
 */
void mqtt_set_ready(void);

/**
 * Call periodically from loop() to process incoming MQTT messages
 * and maintain the connection.  Non-blocking.
 */
void mqtt_loop(void);

/**
 * Publish a setting change that originated locally (touch menu, encoder).
 * Called by radar_ui whenever a user changes a setting.
 */
void mqtt_publish_theme(int theme);
void mqtt_publish_range(float range_nm);
void mqtt_publish_brightness(int brightness);
void mqtt_publish_labels(bool show);
void mqtt_publish_land(bool show);

/**
 * Publish sensor telemetry (called from main loop periodically).
 */
void mqtt_publish_status(int wifi_rssi, int aircraft_count, bool feed_ok);

#ifdef __cplusplus
}
#endif
