#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STATUS_LED_BOOTING,
    STATUS_LED_BLE_PROVISIONING,
    STATUS_LED_WIFI_CONNECTING,
    STATUS_LED_MQTT_CONNECTING,
    STATUS_LED_ONLINE,
    STATUS_LED_OFFLINE,
    STATUS_LED_ERROR,
    STATUS_LED_WIFI_CONNECTED, /* Cyan, IPv4 ready; makes no MQTT claim. */
    STATUS_LED_OFF,
    STATUS_LED_STATE_COUNT,
} status_led_state_t;

/* Task context only. Concurrent init is controlled: while initializing returns
 * INVALID_STATE; after completion returns the original result. No retry/leak.
 * Reserves low-speed LEDC timer 0 and channels 0, 1, 2 for this boot.
 * Component linker fragment keeps LEDC task paths in flash: do not introduce
 * LEDC ISR/fade/cache-off users without reviewing that placement. */
esp_err_t status_led_manager_init(void);

/* Replaces the base state (BOOTING/connectivity/OFF). ERROR and BLE instead
 * assert independent overlays; clear them explicitly with set_condition.
 * Base updates continue beneath overlays. APIs copy state and wake the worker;
 * ESP_OK means accepted, not necessarily already visible on the pins. */
esp_err_t status_led_manager_set_state(status_led_state_t state);
/* Any state may also be asserted/released as an independent condition.
 * ERROR > BLE > OFFLINE/WIFI_CONNECTING > MQTT_CONNECTING > WIFI_CONNECTED >
 * ONLINE > BOOTING > OFF. Each condition must have one owner responsible for
 * clearing it; these are booleans, not reference counts. */
esp_err_t status_led_manager_set_condition(status_led_state_t state, bool active);
/* Atomic replacement of connectivity base and BLE overlay. Preserves ERROR. */
esp_err_t status_led_manager_set_connectivity(status_led_state_t base, bool provisioning);
status_led_state_t status_led_manager_get_state(void);
/* Static diagnostic name; INVALID for an unknown enum value. */
const char *status_led_manager_state_name(status_led_state_t state);
esp_err_t status_led_manager_set_brightness(uint8_t brightness);
/* Hides ONLINE only; faults/provisioning remain visible. */
esp_err_t status_led_manager_set_online_visible(bool visible);

#ifdef __cplusplus
}
#endif
