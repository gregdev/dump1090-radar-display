/**
 * MQTT Manager — bridges radar display settings to Home Assistant via Mosquitto.
 *
 * Uses Home Assistant MQTT auto-discovery so entities appear automatically.
 * Settings sync bidirectionally: changes from HA → ESP32 and vice versa.
 */

#include "mqtt_manager.h"
#include "config.h"
#include "radar_ui.h"

#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

/* Discovery payloads are ~280 bytes — PubSubClient defaults to 256.
 * Must define before the include so the internal buffer is sized correctly. */
#define MQTT_MAX_PACKET_SIZE 512
#include <PubSubClient.h>

/* ── MQTT client + synchronisation ──────────────────────── */
static WiFiClient        wifi_client;
static PubSubClient      mqtt(wifi_client);
static SemaphoreHandle_t mqtt_mutex = NULL;
static TaskHandle_t      mqtt_task_handle = NULL;
static bool              discovery_sent = false;
static volatile bool      mqtt_ready    = false;  /* set true after init complete */
static unsigned long     last_reconnect_attempt = 0;
static unsigned long     last_status_publish   = 0;

/* ── topic helpers ─────────────────────────────────────── */
#define BASE_TOPIC "radar_display"

static const char *theme_cmd      = BASE_TOPIC "/theme/set";
static const char *theme_state    = BASE_TOPIC "/theme/state";
static const char *range_cmd      = BASE_TOPIC "/range/set";
static const char *range_state    = BASE_TOPIC "/range/state";
static const char *brightness_cmd  = BASE_TOPIC "/brightness/set";
static const char *brightness_state = BASE_TOPIC "/brightness/state";
static const char *labels_cmd     = BASE_TOPIC "/labels/set";
static const char *labels_state   = BASE_TOPIC "/labels/state";
static const char *land_cmd       = BASE_TOPIC "/land/set";
static const char *land_state     = BASE_TOPIC "/land/state";
static const char *rssi_state     = BASE_TOPIC "/wifi_rssi/state";
static const char *ac_state       = BASE_TOPIC "/aircraft_count/state";
static const char *feed_state     = BASE_TOPIC "/feed_status/state";

/* ── forward decls ─────────────────────────────────────── */
static void publish_discovery(void);
static void mqtt_reconnect(void);

/* ═══════════════════════════════════════════════════════════
 *  MQTT Callback — handle incoming commands from HA
 * ═══════════════════════════════════════════════════════════ */

static void mqtt_callback(char *topic, byte *payload, unsigned int length) {
    /* null-terminate payload for safe string handling */
    char buf[16] = {0};
    unsigned int copy_len = length < sizeof(buf) - 1 ? length : sizeof(buf) - 1;
    memcpy(buf, payload, copy_len);
    buf[copy_len] = '\0';

    if (strcmp(topic, theme_cmd) == 0) {
        int t = atoi(buf);
        if (t >= 0 && t <= 3) {
            radar_ui_set_theme(t);
            mqtt.publish(theme_state, buf, true);
            Serial.printf("MQTT: theme → %d\n", t);
        }
    }
    else if (strcmp(topic, range_cmd) == 0) {
        float r = atof(buf);
        if (r >= 10.0f && r <= 80.0f) {
            radar_ui_set_range(r);
            mqtt.publish(range_state, buf, true);
            Serial.printf("MQTT: range → %.0f NM\n", (double)r);
        }
    }
    else if (strcmp(topic, brightness_cmd) == 0) {
        int b = atoi(buf);
        if (b >= 1 && b <= 100) {
            radar_ui_set_brightness(b);
            mqtt.publish(brightness_state, buf, true);
            Serial.printf("MQTT: brightness → %d%%\n", b);
        }
    }
    else if (strcmp(topic, labels_cmd) == 0) {
        bool on = (buf[0] == 'O' || buf[0] == 'o' || buf[0] == '1' || strcmp(buf, "ON") == 0);
        radar_ui_set_labels(on);
        mqtt.publish(labels_state, on ? "ON" : "OFF", true);
        Serial.printf("MQTT: labels → %s\n", on ? "ON" : "OFF");
    }
    else if (strcmp(topic, land_cmd) == 0) {
        bool on = (buf[0] == 'O' || buf[0] == 'o' || buf[0] == '1' || strcmp(buf, "ON") == 0);
        radar_ui_set_land(on);
        mqtt.publish(land_state, on ? "ON" : "OFF", true);
        Serial.printf("MQTT: land → %s\n", on ? "ON" : "OFF");
    }
}

/* ═══════════════════════════════════════════════════════════
 *  Home Assistant Auto-Discovery
 * ═══════════════════════════════════════════════════════════ */

static void publish_discovery(void) {
    if (discovery_sent) return;

    static char payload[512];  /* static: lives in BSS, not stack */
    bool all_ok = true;

    /* ── device info (reused in every entity) ── */
    #define DEV_JSON \
        "\"dev\":{" \
            "\"ids\":[\"radar_display_esp32\"]," \
            "\"name\":\"Radar Display\"," \
            "\"mf\":\"ESP32\"," \
            "\"mdl\":\"dump1090 Radar\"," \
            "\"sw\":\"1.0\"" \
        "}"

    #define PUB_OR_FAIL(topic) do { \
        Serial.printf("MQTT: pub buf=%u state=%d conn=%d\n", \
                       mqtt.getBufferSize(), mqtt.state(), mqtt.connected()); \
        bool ok = mqtt.publish(topic, payload, true); \
        if (!ok) { \
            Serial.print("MQTT: publish FAILED for "); \
            Serial.println(topic); \
            all_ok = false; \
        } else { \
            Serial.print("MQTT: publish OK for "); \
            Serial.println(topic); \
        } \
        mqtt.loop(); /* flush TCP — PubSubClient needs this between rapid publishes */ \
    } while(0)

    /* ── Select: Colour Theme ── */
    snprintf(payload, sizeof(payload),
        "{\"name\":\"Colour Theme\",\"uniq_id\":\"radar_disp_theme\","
        "\"cmd_t\":\"%s\",\"stat_t\":\"%s\","
        "\"options\":[\"Green\",\"Blue\",\"Red\",\"Amber\"],"
        "\"ret\":true," DEV_JSON "}",
        theme_cmd, theme_state);
    Serial.printf("MQTT: discovery payload %zu bytes\n", strlen(payload));
    PUB_OR_FAIL("homeassistant/select/radar_display/theme/config");

    /* ── Select: Range ── */
    snprintf(payload, sizeof(payload),
        "{\"name\":\"Range\",\"uniq_id\":\"radar_disp_range\","
        "\"cmd_t\":\"%s\",\"stat_t\":\"%s\","
        "\"options\":[\"10\",\"20\",\"40\",\"80\"],"
        "\"ret\":true," DEV_JSON "}",
        range_cmd, range_state);
    PUB_OR_FAIL("homeassistant/select/radar_display/range/config");

    /* ── Number: Brightness ── */
    snprintf(payload, sizeof(payload),
        "{\"name\":\"Brightness\",\"uniq_id\":\"radar_disp_brightness\","
        "\"cmd_t\":\"%s\",\"stat_t\":\"%s\","
        "\"min\":1,\"max\":100,\"step\":1,\"mode\":\"slider\","
        "\"unit_of_meas\":\"%%\",\"ret\":true," DEV_JSON "}",
        brightness_cmd, brightness_state);
    PUB_OR_FAIL("homeassistant/number/radar_display/brightness/config");

    /* ── Switch: Show Labels ── */
    snprintf(payload, sizeof(payload),
        "{\"name\":\"Show Labels\",\"uniq_id\":\"radar_disp_labels\","
        "\"cmd_t\":\"%s\",\"stat_t\":\"%s\","
        "\"ret\":true," DEV_JSON "}",
        labels_cmd, labels_state);
    PUB_OR_FAIL("homeassistant/switch/radar_display/labels/config");

    /* ── Switch: Show Land ── */
    snprintf(payload, sizeof(payload),
        "{\"name\":\"Show Land\",\"uniq_id\":\"radar_disp_land\","
        "\"cmd_t\":\"%s\",\"stat_t\":\"%s\","
        "\"ret\":true," DEV_JSON "}",
        land_cmd, land_state);
    PUB_OR_FAIL("homeassistant/switch/radar_display/land/config");

    /* ── Sensor: WiFi RSSI ── */
    snprintf(payload, sizeof(payload),
        "{\"name\":\"WiFi RSSI\",\"uniq_id\":\"radar_disp_rssi\","
        "\"stat_t\":\"%s\","
        "\"unit_of_meas\":\"dBm\",\"dev_cla\":\"signal_strength\","
        "\"ret\":true," DEV_JSON "}",
        rssi_state);
    PUB_OR_FAIL("homeassistant/sensor/radar_display/wifi_rssi/config");

    /* ── Sensor: Aircraft Count ── */
    snprintf(payload, sizeof(payload),
        "{\"name\":\"Aircraft Count\",\"uniq_id\":\"radar_disp_aircraft\","
        "\"stat_t\":\"%s\","
        "\"ret\":true," DEV_JSON "}",
        ac_state);
    PUB_OR_FAIL("homeassistant/sensor/radar_display/aircraft_count/config");

    /* ── Binary Sensor: Feed Status ── */
    snprintf(payload, sizeof(payload),
        "{\"name\":\"Feed Status\",\"uniq_id\":\"radar_disp_feed\","
        "\"stat_t\":\"%s\","
        "\"dev_cla\":\"connectivity\",\"ret\":true," DEV_JSON "}",
        feed_state);
    PUB_OR_FAIL("homeassistant/binary_sensor/radar_display/feed_status/config");

    #undef DEV_JSON
    #undef PUB_OR_FAIL

    if (all_ok) {
        discovery_sent = true;
        Serial.println("MQTT: discovery OK (8/8)");
    } else {
        Serial.println("MQTT: discovery had failures — will retry next reconnect");
        discovery_sent = false;  /* retry on next connect */
    }
}

/* ═══════════════════════════════════════════════════════════
 *  Reconnect
 * ═══════════════════════════════════════════════════════════ */

static void mqtt_reconnect(void) {
    if (mqtt.connected()) return;

    unsigned long now = millis();
    if (now - last_reconnect_attempt < 5000) return;
    last_reconnect_attempt = now;

    Serial.print("MQTT: connecting to ");
    Serial.print(MQTT_BROKER_HOST);
    Serial.print(" as '");
    Serial.print(MQTT_USERNAME);
    Serial.print("' ... ");

    /* Use a unique client ID based on MAC address */
    char client_id[32];
    snprintf(client_id, sizeof(client_id), "radar_display_%06X",
             (uint32_t)(ESP.getEfuseMac() & 0xFFFFFF));

    if (mqtt.connect(client_id, MQTT_USERNAME, MQTT_PASSWORD)) {
        Serial.println("connected");

        /* Subscribe to command topics */
        mqtt.subscribe(theme_cmd);
        mqtt.subscribe(range_cmd);
        mqtt.subscribe(brightness_cmd);
        mqtt.subscribe(labels_cmd);
        mqtt.subscribe(land_cmd);

        /* Publish auto-discovery (retained — HA picks them up) */
        publish_discovery();

        /* Publish current state so HA reflects actual settings */
        mqtt_publish_theme(radar_ui_get_theme());
        mqtt_publish_range(radar_ui_get_range());
        mqtt_publish_brightness(radar_ui_get_brightness());
        mqtt_publish_labels(radar_ui_get_show_labels());
        mqtt_publish_land(radar_ui_get_show_land());
    } else {
        Serial.print("failed, rc=");
        Serial.println(mqtt.state());
    }
}

/* ═══════════════════════════════════════════════════════════
 *  MQTT Task — runs on its own FreeRTOS thread (8 KB stack)
 * ═══════════════════════════════════════════════════════════ */

static void mqtt_task(void *pvParameters) {
    (void)pvParameters;

    /* Wait for setup() to finish initialisation before touching WiFi / MQTT */
    while (!mqtt_ready) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    Serial.println("MQTT: task started");

    while (1) {
        if (WiFi.status() == WL_CONNECTED) {
            if (xSemaphoreTakeRecursive(mqtt_mutex, pdMS_TO_TICKS(100))) {
                if (!mqtt.connected()) {
                    mqtt_reconnect();
                }
                mqtt.loop();
                xSemaphoreGiveRecursive(mqtt_mutex);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/* ═══════════════════════════════════════════════════════════
 *  Public API
 * ═══════════════════════════════════════════════════════════ */

void mqtt_init(void) {
    mqtt.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    mqtt.setCallback(mqtt_callback);
    mqtt.setKeepAlive(60);
    mqtt.setBufferSize(512);

    mqtt_mutex = xSemaphoreCreateRecursiveMutex();
    if (!mqtt_mutex) {
        Serial.println("MQTT: FATAL — mutex create failed");
        return;
    }

    Serial.printf("MQTT: broker %s:%d (buf=%u)\n", MQTT_BROKER_HOST, MQTT_BROKER_PORT,
                   mqtt.getBufferSize());

    BaseType_t ret = xTaskCreate(
        mqtt_task, "mqtt", 8192, NULL,
        uxTaskPriorityGet(NULL),
        &mqtt_task_handle);
    if (ret != pdPASS) {
        Serial.println("MQTT: FATAL — task create failed");
    }
}

void mqtt_loop(void) {
    /* No-op: MQTT runs on its own FreeRTOS task now. */
}

void mqtt_set_ready(void) {
    mqtt_ready = true;
}

/* ── Publish helpers (mutex guarded for cross-task safety) ── */

void mqtt_publish_theme(int theme) {
    static const char *names[] = {"Green", "Blue", "Red", "Amber"};
    if (theme < 0 || theme >= 4) return;
    if (mqtt_mutex && xSemaphoreTakeRecursive(mqtt_mutex, pdMS_TO_TICKS(500))) {
        mqtt.publish(theme_state, names[theme], true);
        xSemaphoreGiveRecursive(mqtt_mutex);
    }
}

void mqtt_publish_range(float range_nm) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%.0f", (double)range_nm);
    if (mqtt_mutex && xSemaphoreTakeRecursive(mqtt_mutex, pdMS_TO_TICKS(500))) {
        mqtt.publish(range_state, buf, true);
        xSemaphoreGiveRecursive(mqtt_mutex);
    }
}

void mqtt_publish_brightness(int brightness) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", brightness);
    if (mqtt_mutex && xSemaphoreTakeRecursive(mqtt_mutex, pdMS_TO_TICKS(500))) {
        mqtt.publish(brightness_state, buf, true);
        xSemaphoreGiveRecursive(mqtt_mutex);
    }
}

void mqtt_publish_labels(bool show) {
    if (mqtt_mutex && xSemaphoreTakeRecursive(mqtt_mutex, pdMS_TO_TICKS(500))) {
        mqtt.publish(labels_state, show ? "ON" : "OFF", true);
        xSemaphoreGiveRecursive(mqtt_mutex);
    }
}

void mqtt_publish_land(bool show) {
    if (mqtt_mutex && xSemaphoreTakeRecursive(mqtt_mutex, pdMS_TO_TICKS(500))) {
        mqtt.publish(land_state, show ? "ON" : "OFF", true);
        xSemaphoreGiveRecursive(mqtt_mutex);
    }
}

void mqtt_publish_status(int wifi_rssi, int aircraft_count, bool feed_ok) {
    unsigned long now = millis();
    if (now - last_status_publish < 30000) return;
    last_status_publish = now;

    char buf[16];
    if (!mqtt_mutex) return;

    if (xSemaphoreTakeRecursive(mqtt_mutex, pdMS_TO_TICKS(500))) {
        snprintf(buf, sizeof(buf), "%d", wifi_rssi);
        mqtt.publish(rssi_state, buf, true);

        snprintf(buf, sizeof(buf), "%d", aircraft_count);
        mqtt.publish(ac_state, buf, true);

        mqtt.publish(feed_state, feed_ok ? "ON" : "OFF", true);

        xSemaphoreGiveRecursive(mqtt_mutex);
    }
}
