#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif_ip_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DECLARE_BASE(WIFI_MANAGER_EVENT);

typedef enum {
    WIFI_MANAGER_EVENT_CONNECTED,             /* IPv4 is ready, not just associated. */
    WIFI_MANAGER_EVENT_DISCONNECTED,
    WIFI_MANAGER_EVENT_PROVISIONING_STARTED,
    WIFI_MANAGER_EVENT_PROVISIONING_SUCCESS,
    WIFI_MANAGER_EVENT_PROVISIONING_FAILED,
    WIFI_MANAGER_EVENT_CONNECTING,            /* A station attempt is starting. */
    WIFI_MANAGER_EVENT_PROVISIONING_ENDED,    /* SDK resources released. */
} wifi_manager_event_id_t;

typedef struct {
    esp_ip4_addr_t ip;
} wifi_manager_connected_event_t;

typedef struct {
    /* ESP-IDF wifi_err_reason_t; zero means IPv4 lost or station stopped. */
    uint8_t reason;
} wifi_manager_disconnected_event_t;

typedef enum {
    WIFI_MANAGER_PROV_AUTH_ERROR,
    WIFI_MANAGER_PROV_AP_NOT_FOUND,
    WIFI_MANAGER_PROV_INTERNAL_ERROR,
} wifi_manager_provisioning_failure_reason_t;

typedef struct {
    wifi_manager_provisioning_failure_reason_t reason;
    esp_err_t error; /* ESP_FAIL for rejected credentials; SDK error otherwise. */
} wifi_manager_provisioning_failed_event_t;

/** Initialize NVS, default netif/event loop, Wi-Fi and the official manager.
 * NVS errors are returned without erasing anything. Call from one startup task.
 * Repeated calls return the original result, including initialization failures.
 * This module owns the Wi-Fi driver.
 */
esp_err_t wifi_manager_init(void);

/** Start saved Wi-Fi or provisioning, without waiting for credentials or IP.
 * Call after init, from the same startup task (not an event callback).
 * Repeated calls return the first startup result. SDK work runs on a worker.
 */
esp_err_t wifi_manager_start(void);

/** Queue an explicit Wi-Fi-only reset and restart BLE provisioning without reboot.
 * Task-context API, safe from event callbacks. ESP_OK means accepted, not completed.
 * A concurrent request returns ESP_ERR_INVALID_STATE; queue congestion is returned.
 * Observe PROVISIONING_STARTED / PROVISIONING_FAILED for the asynchronous outcome.
 * May retry after a failure. Does not erase other NVS namespaces or Wi-Fi settings.
 */
esp_err_t wifi_manager_reset_credentials(void);

/** Thread-safe snapshots; false before initialization. Connected means IPv4 ready. */
bool wifi_manager_is_connected(void);
bool wifi_manager_is_provisioning(void);

/** Saved credentials found at boot, or initial provisioning validated with IP.
 * This does not guarantee the AP is currently available. Never cleared by an
 * ordinary disconnection. During initial credential validation it remains false.
 */
bool wifi_manager_is_provisioned(void);

/* Register on the default event loop after init and before start:
 * esp_event_handler_register(WIFI_MANAGER_EVENT, ESP_EVENT_ANY_ID, handler, ctx);
 * Payloads are copied by esp_event; their pointers are valid only in the callback.
 * STARTED/SUCCESS have no payload. Keep callbacks short. Notifications are best
 * effort if the default queue is full; use the snapshot API to reconcile state.
 */

#ifdef __cplusplus
}
#endif
