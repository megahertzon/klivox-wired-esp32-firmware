/* Wi-Fi Provisioning Manager Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <stdint.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include "wifi_manager.h"
#include <esp_check.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_wifi_default.h>
#include <esp_event.h>
#include <nvs_flash.h>

#include <wifi_provisioning/manager.h>

#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_BLE
#include <wifi_provisioning/scheme_ble.h>
#endif /* CONFIG_EXAMPLE_PROV_TRANSPORT_BLE */

#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_SOFTAP
#include <wifi_provisioning/scheme_softap.h>
#endif /* CONFIG_EXAMPLE_PROV_TRANSPORT_SOFTAP */

static const char *TAG = "KLIVOX_WIFI";

#if CONFIG_EXAMPLE_PROV_SECURITY_VERSION_2
#if CONFIG_EXAMPLE_PROV_SEC2_DEV_MODE
#define EXAMPLE_PROV_SEC2_USERNAME          "wifiprov"
#define EXAMPLE_PROV_SEC2_PWD               "abcd1234"

/* This salt,verifier has been generated for username = "wifiprov" and password = "abcd1234"
 * IMPORTANT NOTE: For production cases, this must be unique to every device
 * and should come from device manufacturing partition.*/
static const char sec2_salt[] = {
    0x03, 0x6e, 0xe0, 0xc7, 0xbc, 0xb9, 0xed, 0xa8, 0x4c, 0x9e, 0xac, 0x97, 0xd9, 0x3d, 0xec, 0xf4
};

static const char sec2_verifier[] = {
    0x7c, 0x7c, 0x85, 0x47, 0x65, 0x08, 0x94, 0x6d, 0xd6, 0x36, 0xaf, 0x37, 0xd7, 0xe8, 0x91, 0x43,
    0x78, 0xcf, 0xfd, 0x61, 0x6c, 0x59, 0xd2, 0xf8, 0x39, 0x08, 0x12, 0x72, 0x38, 0xde, 0x9e, 0x24,
    0xa4, 0x70, 0x26, 0x1c, 0xdf, 0xa9, 0x03, 0xc2, 0xb2, 0x70, 0xe7, 0xb1, 0x32, 0x24, 0xda, 0x11,
    0x1d, 0x97, 0x18, 0xdc, 0x60, 0x72, 0x08, 0xcc, 0x9a, 0xc9, 0x0c, 0x48, 0x27, 0xe2, 0xae, 0x89,
    0xaa, 0x16, 0x25, 0xb8, 0x04, 0xd2, 0x1a, 0x9b, 0x3a, 0x8f, 0x37, 0xf6, 0xe4, 0x3a, 0x71, 0x2e,
    0xe1, 0x27, 0x86, 0x6e, 0xad, 0xce, 0x28, 0xff, 0x54, 0x46, 0x60, 0x1f, 0xb9, 0x96, 0x87, 0xdc,
    0x57, 0x40, 0xa7, 0xd4, 0x6c, 0xc9, 0x77, 0x54, 0xdc, 0x16, 0x82, 0xf0, 0xed, 0x35, 0x6a, 0xc4,
    0x70, 0xad, 0x3d, 0x90, 0xb5, 0x81, 0x94, 0x70, 0xd7, 0xbc, 0x65, 0xb2, 0xd5, 0x18, 0xe0, 0x2e,
    0xc3, 0xa5, 0xf9, 0x68, 0xdd, 0x64, 0x7b, 0xb8, 0xb7, 0x3c, 0x9c, 0xfc, 0x00, 0xd8, 0x71, 0x7e,
    0xb7, 0x9a, 0x7c, 0xb1, 0xb7, 0xc2, 0xc3, 0x18, 0x34, 0x29, 0x32, 0x43, 0x3e, 0x00, 0x99, 0xe9,
    0x82, 0x94, 0xe3, 0xd8, 0x2a, 0xb0, 0x96, 0x29, 0xb7, 0xdf, 0x0e, 0x5f, 0x08, 0x33, 0x40, 0x76,
    0x52, 0x91, 0x32, 0x00, 0x9f, 0x97, 0x2c, 0x89, 0x6c, 0x39, 0x1e, 0xc8, 0x28, 0x05, 0x44, 0x17,
    0x3f, 0x68, 0x02, 0x8a, 0x9f, 0x44, 0x61, 0xd1, 0xf5, 0xa1, 0x7e, 0x5a, 0x70, 0xd2, 0xc7, 0x23,
    0x81, 0xcb, 0x38, 0x68, 0xe4, 0x2c, 0x20, 0xbc, 0x40, 0x57, 0x76, 0x17, 0xbd, 0x08, 0xb8, 0x96,
    0xbc, 0x26, 0xeb, 0x32, 0x46, 0x69, 0x35, 0x05, 0x8c, 0x15, 0x70, 0xd9, 0x1b, 0xe9, 0xbe, 0xcc,
    0xa9, 0x38, 0xa6, 0x67, 0xf0, 0xad, 0x50, 0x13, 0x19, 0x72, 0x64, 0xbf, 0x52, 0xc2, 0x34, 0xe2,
    0x1b, 0x11, 0x79, 0x74, 0x72, 0xbd, 0x34, 0x5b, 0xb1, 0xe2, 0xfd, 0x66, 0x73, 0xfe, 0x71, 0x64,
    0x74, 0xd0, 0x4e, 0xbc, 0x51, 0x24, 0x19, 0x40, 0x87, 0x0e, 0x92, 0x40, 0xe6, 0x21, 0xe7, 0x2d,
    0x4e, 0x37, 0x76, 0x2f, 0x2e, 0xe2, 0x68, 0xc7, 0x89, 0xe8, 0x32, 0x13, 0x42, 0x06, 0x84, 0x84,
    0x53, 0x4a, 0xb3, 0x0c, 0x1b, 0x4c, 0x8d, 0x1c, 0x51, 0x97, 0x19, 0xab, 0xae, 0x77, 0xff, 0xdb,
    0xec, 0xf0, 0x10, 0x95, 0x34, 0x33, 0x6b, 0xcb, 0x3e, 0x84, 0x0f, 0xb9, 0xd8, 0x5f, 0xb8, 0xa0,
    0xb8, 0x55, 0x53, 0x3e, 0x70, 0xf7, 0x18, 0xf5, 0xce, 0x7b, 0x4e, 0xbf, 0x27, 0xce, 0xce, 0xa8,
    0xb3, 0xbe, 0x40, 0xc5, 0xc5, 0x32, 0x29, 0x3e, 0x71, 0x64, 0x9e, 0xde, 0x8c, 0xf6, 0x75, 0xa1,
    0xe6, 0xf6, 0x53, 0xc8, 0x31, 0xa8, 0x78, 0xde, 0x50, 0x40, 0xf7, 0x62, 0xde, 0x36, 0xb2, 0xba
};
#endif

static esp_err_t get_sec2_salt(const char **salt, uint16_t *salt_len) {
#if CONFIG_EXAMPLE_PROV_SEC2_DEV_MODE
    ESP_LOGI(TAG, "Development mode: using hard coded salt");
    *salt = sec2_salt;
    *salt_len = sizeof(sec2_salt);
    return ESP_OK;
#elif CONFIG_EXAMPLE_PROV_SEC2_PROD_MODE
    ESP_LOGE(TAG, "Production Security 2 requires per-device factory salt");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

static esp_err_t get_sec2_verifier(const char **verifier, uint16_t *verifier_len) {
#if CONFIG_EXAMPLE_PROV_SEC2_DEV_MODE
    ESP_LOGI(TAG, "Development mode: using hard coded verifier");
    *verifier = sec2_verifier;
    *verifier_len = sizeof(sec2_verifier);
    return ESP_OK;
#elif CONFIG_EXAMPLE_PROV_SEC2_PROD_MODE
    /* This code needs to be updated with appropriate implementation to provide verifier */
    ESP_LOGE(TAG, "Production Security 2 requires per-device factory verifier");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
#endif



ESP_EVENT_DEFINE_BASE(WIFI_MANAGER_EVENT);
static ESP_EVENT_DEFINE_BASE(WIFI_MANAGER_INTERNAL_EVENT);
enum { RECONNECT_DUE, RESET_REQUEST, RESET_DRAINED, PROVISIONING_RELEASED };
#define WORK_START BIT0
#define WORK_END BIT1
#define WORK_RESET BIT2

#define CONNECTED_BIT       BIT0
#define PROVISIONED_BIT     BIT1
#define PROVISIONING_BIT    BIT2
#define RECONNECT_BIT       BIT3
#define STA_RUNNING_BIT    BIT4
#define ASSOCIATED_BIT     BIT5
#define MANAGER_BIT        BIT6
#define RESET_BIT          BIT7
#define DRAINED_BIT        BIT8
#define START_DONE_BIT     BIT9

static EventGroupHandle_t s_state;
static TaskHandle_t s_control_task;
static portMUX_TYPE s_request_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_reset_pending;
static esp_err_t init_provisioning_manager(void);
static esp_err_t start_provisioning(void);
static esp_err_t start_connectivity(void);
static void control_task(void *arg);
static esp_timer_handle_t s_reconnect_timer;
static esp_netif_t *s_sta_netif;
#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_SOFTAP
static esp_netif_t *s_ap_netif;
#endif
static bool s_wifi_initialized;
static bool s_owns_event_loop;
static bool s_init_attempted;
static bool s_start_attempted;
static esp_err_t s_init_result = ESP_ERR_INVALID_STATE;
static esp_err_t s_start_result = ESP_ERR_INVALID_STATE;
/* Event-loop backoff; the worker resets it only with reconnect disabled. */
static uint32_t s_retry_delay_ms = CONFIG_KLIVOX_WIFI_RETRY_INITIAL_MS;

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data);

static void release_provisioning_manager(void)
{
    /* Atomically take ownership, including startup-error / END overlap. */
    if (s_state && (xEventGroupClearBits(s_state, MANAGER_BIT) & MANAGER_BIT)) {
        wifi_prov_mgr_deinit();
    }
}

static struct {
    esp_event_base_t const *base;
    esp_event_handler_instance_t instance;
} s_handlers[] = {
    { &WIFI_EVENT, NULL },
    { &IP_EVENT, NULL },
    { &WIFI_PROV_EVENT, NULL },
#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_BLE
    { &PROTOCOMM_TRANSPORT_BLE_EVENT, NULL },
#endif
    { &PROTOCOMM_SECURITY_SESSION_EVENT, NULL },
    { &WIFI_MANAGER_INTERNAL_EVENT, NULL },
};
static size_t s_registered_handlers;

static void publish(int32_t id, const void *data, size_t size)
{
    /* Never wait for our own default event-loop queue to drain. */
    esp_err_t err = esp_event_post(WIFI_MANAGER_EVENT, id, data, size, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Notification %ld dropped: %s", (long)id, esp_err_to_name(err));
    }
}

static void cancel_reconnect(void)
{
    /* ESP_ERR_INVALID_STATE just means the one-shot timer already expired. */
    (void)esp_timer_stop(s_reconnect_timer);
}

static bool can_reconnect(void)
{
    EventBits_t bits = xEventGroupGetBits(s_state);
    return (bits & (RECONNECT_BIT | STA_RUNNING_BIT)) == (RECONNECT_BIT | STA_RUNNING_BIT)
           && !(bits & (PROVISIONING_BIT | ASSOCIATED_BIT | RESET_BIT));
}

static void schedule_reconnect(void)
{
    if (!can_reconnect() || esp_timer_is_active(s_reconnect_timer)) {
        return;
    }
    esp_err_t err = esp_timer_start_once(s_reconnect_timer, (uint64_t)s_retry_delay_ms * 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Cannot schedule reconnect: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "Reconnecting in %lu ms", (unsigned long)s_retry_delay_ms);
    s_retry_delay_ms = s_retry_delay_ms > CONFIG_KLIVOX_WIFI_RETRY_MAX_MS / 2
                       ? CONFIG_KLIVOX_WIFI_RETRY_MAX_MS : s_retry_delay_ms * 2;
}

static void reconnect_timer_callback(void *arg)
{
    /* esp_timer's task only posts work. Wi-Fi calls and state changes run on
     * the default loop, serially with disconnection/provisioning events. */
    esp_err_t err = esp_event_post(WIFI_MANAGER_INTERNAL_EVENT, RECONNECT_DUE, NULL, 0, 0);
    if (err != ESP_OK) {
        /* Queue congestion must not permanently disable reconnection. */
        esp_err_t retry = esp_timer_start_once(s_reconnect_timer, 1000000);
        if (retry != ESP_OK) {
            ESP_LOGE(TAG, "Cannot retry reconnect notification: %s", esp_err_to_name(retry));
        }
    }
}

static void connect_station(void)
{
    if (!can_reconnect()) {
        return;
    }
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Connection request failed: %s", esp_err_to_name(err));
        schedule_reconnect();
    }
}

static void mark_disconnected(uint8_t reason, bool always_notify)
{
    EventBits_t previous = xEventGroupClearBits(s_state, CONNECTED_BIT);
    if (always_notify || (previous & CONNECTED_BIT)) {
        wifi_manager_disconnected_event_t event = { .reason = reason };
        publish(WIFI_MANAGER_EVENT_DISCONNECTED, &event, sizeof(event));
    }
}

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_MANAGER_INTERNAL_EVENT && id == RESET_REQUEST) {
        xEventGroupSetBits(s_state, RESET_BIT);
        xEventGroupClearBits(s_state, RECONNECT_BIT);
        cancel_reconnect();
        xTaskNotify(s_control_task, WORK_RESET, eSetBits);
        return;
    }
    if (base == WIFI_MANAGER_INTERNAL_EVENT && id == RESET_DRAINED) {
        /* FIFO barrier: all old END, STOP and IP events have been consumed. */
        xEventGroupSetBits(s_state, DRAINED_BIT);
        return;
    }
    if (base == WIFI_MANAGER_INTERNAL_EVENT && id == PROVISIONING_RELEASED) {
        if (!(xEventGroupGetBits(s_state) & RESET_BIT)) {
            xEventGroupClearBits(s_state, PROVISIONING_BIT);
            if (xEventGroupGetBits(s_state) & PROVISIONED_BIT) {
                xEventGroupSetBits(s_state, RECONNECT_BIT);
                schedule_reconnect();
            }
        }
        return;
    }
    if ((xEventGroupGetBits(s_state) & RESET_BIT) &&
        (base == WIFI_PROV_EVENT || base == IP_EVENT)) {
        return;
    }
    if (base == WIFI_MANAGER_INTERNAL_EVENT && id == RECONNECT_DUE) {
        connect_station();
    } else if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            xEventGroupSetBits(s_state, STA_RUNNING_BIT);
            connect_station();
            break;
        case WIFI_EVENT_STA_CONNECTED:
            xEventGroupSetBits(s_state, ASSOCIATED_BIT);
            cancel_reconnect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED: {
            wifi_event_sta_disconnected_t *event = data;
            xEventGroupClearBits(s_state, ASSOCIATED_BIT);
            mark_disconnected(event->reason, true);
            ESP_LOGW(TAG, "Wi-Fi disconnected (reason %u)", event->reason);
            /* The official manager owns all attempts during provisioning. */
            schedule_reconnect();
            break;
        }
        case WIFI_EVENT_STA_STOP:
            xEventGroupClearBits(s_state, STA_RUNNING_BIT | ASSOCIATED_BIT);
            cancel_reconnect();
            mark_disconnected(0, false);
            break;
        default:
            break;
        }
    } else if (base == IP_EVENT) {
        if (id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *event = data;
            if (event->esp_netif != s_sta_netif) {
                return;
            }
            cancel_reconnect();
            s_retry_delay_ms = CONFIG_KLIVOX_WIFI_RETRY_INITIAL_MS;
            xEventGroupSetBits(s_state, CONNECTED_BIT);
            ESP_LOGI(TAG, "Connected with IP address: " IPSTR, IP2STR(&event->ip_info.ip));
            wifi_manager_connected_event_t connected = { .ip = event->ip_info.ip };
            publish(WIFI_MANAGER_EVENT_CONNECTED, &connected, sizeof(connected));
        } else if (id == IP_EVENT_STA_LOST_IP) {
            /* The driver remains associated; DHCP can recover without reconnecting. */
            mark_disconnected(0, false);
        }
    } else if (base == WIFI_PROV_EVENT) {
        switch (id) {
        case WIFI_PROV_START:
            ESP_LOGI(TAG, "Provisioning started");
            publish(WIFI_MANAGER_EVENT_PROVISIONING_STARTED, NULL, 0);
            break;
        case WIFI_PROV_CRED_RECV:
            ESP_LOGI(TAG, "Received Wi-Fi credentials (not logged)");
            break;
        case WIFI_PROV_CRED_SUCCESS:
            if (!(xEventGroupGetBits(s_state) & PROVISIONED_BIT)) {
                xEventGroupSetBits(s_state, PROVISIONED_BIT);
                ESP_LOGI(TAG, "Provisioning successful");
                publish(WIFI_MANAGER_EVENT_PROVISIONING_SUCCESS, NULL, 0);
            }
            break;
        case WIFI_PROV_CRED_FAIL: {
            /* The SDK still owns Wi-Fi until END. A temporary loss after a
             * successful validation must never reset the saved credentials. */
            if (xEventGroupGetBits(s_state) & PROVISIONED_BIT) {
                wifi_prov_mgr_stop_provisioning();
                break;
            }
            wifi_prov_sta_fail_reason_t reason = *(wifi_prov_sta_fail_reason_t *)data;
            wifi_manager_provisioning_failed_event_t failure = {
                .reason = reason == WIFI_PROV_STA_AUTH_ERROR
                          ? WIFI_MANAGER_PROV_AUTH_ERROR : WIFI_MANAGER_PROV_AP_NOT_FOUND,
                .error = ESP_FAIL,
            };
            ESP_LOGW(TAG, "Initial provisioning failed; correct credentials in the app");
            publish(WIFI_MANAGER_EVENT_PROVISIONING_FAILED, &failure, sizeof(failure));
#ifdef CONFIG_EXAMPLE_RESET_PROV_MGR_ON_FAILURE
            /* Preserve the example's retry behavior only for NEW rejected
             * credentials, never for an established installation. */
            esp_err_t err = wifi_prov_mgr_reset_sm_state_on_failure();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Cannot allow provisioning retry: %s", esp_err_to_name(err));
            }
#endif
            break;
        }
        case WIFI_PROV_END:
            xTaskNotify(s_control_task, WORK_END, eSetBits);
            break;
        default:
            break;
        }
#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_BLE
    } else if (base == PROTOCOMM_TRANSPORT_BLE_EVENT) {
        if (id == PROTOCOMM_TRANSPORT_BLE_CONNECTED) {
            ESP_LOGI(TAG, "BLE transport connected");
        } else if (id == PROTOCOMM_TRANSPORT_BLE_DISCONNECTED) {
            ESP_LOGI(TAG, "BLE transport disconnected");
        }
#endif
    } else if (base == PROTOCOMM_SECURITY_SESSION_EVENT) {
        if (id == PROTOCOMM_SECURITY_SESSION_SETUP_OK) {
            ESP_LOGI(TAG, "Secured provisioning session established");
        } else if (id == PROTOCOMM_SECURITY_SESSION_INVALID_SECURITY_PARAMS ||
                   id == PROTOCOMM_SECURITY_SESSION_CREDENTIALS_MISMATCH) {
            ESP_LOGW(TAG, "Provisioning secure session rejected");
        }
    }
}

static esp_err_t get_device_service_name(char *service_name, size_t max)
{
    uint8_t eth_mac[6];
    const char *ssid_prefix = "PROV_";
    ESP_RETURN_ON_ERROR(esp_wifi_get_mac(WIFI_IF_STA, eth_mac), TAG, "Cannot read STA MAC");
    snprintf(service_name, max, "%s%02X%02X%02X",
             ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);
    return ESP_OK;
}

/* Handler for the optional provisioning endpoint registered by the application.
 * The data format can be chosen by applications. Here, we are using plain ascii text.
 * Applications can choose to use other formats like protobuf, JSON, XML, etc.
 * Note that memory for the response buffer must be allocated using heap as this buffer
 * gets freed by the protocomm layer once it has been sent by the transport layer.
 */
static esp_err_t custom_prov_data_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen,
                                          uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
    /* Do not log arbitrary client data: it may contain secrets. */
    char response[] = "SUCCESS";
    *outbuf = (uint8_t *)strdup(response);
    if (*outbuf == NULL) {
        ESP_LOGE(TAG, "System out of memory");
        return ESP_ERR_NO_MEM;
    }
    *outlen = strlen(response) + 1; /* +1 for NULL terminating byte */

    return ESP_OK;
}

#ifdef CONFIG_EXAMPLE_PROV_ENABLE_APP_CALLBACK
static void wifi_prov_app_callback(void *user_data, wifi_prov_cb_event_t event, void *event_data)
{
    /**
     * This is blocking callback, any configurations that needs to be set when a particular
     * provisioning event is triggered can be set here.
    */
    switch (event) {
        case WIFI_PROV_SET_STA_CONFIG: {
            /**
             * Wi-Fi configurations can be set here before the Wi-Fi is enabled in
             * STA mode.
            */
            wifi_config_t *wifi_config = (wifi_config_t*)event_data;
            (void) wifi_config;
            break;
        }
        default:
            break;
    }
}

static const wifi_prov_event_handler_t wifi_prov_event_handler = {
    .event_cb = wifi_prov_app_callback,
    .user_data = NULL,
};
#endif /* EXAMPLE_PROV_ENABLE_APP_CALLBACK */


static esp_err_t start_provisioning(void)
{
    char service_name[12];
    ESP_RETURN_ON_ERROR(get_device_service_name(service_name, sizeof(service_name)), TAG, "Cannot create service name");

#ifdef CONFIG_EXAMPLE_PROV_SECURITY_VERSION_1
    /* What is the security level that we want (0, 1, 2):
     *      - WIFI_PROV_SECURITY_0 is simply plain text communication.
     *      - WIFI_PROV_SECURITY_1 is secure communication which consists of secure handshake
     *          using X25519 key exchange and proof of possession (pop) and AES-CTR
     *          for encryption/decryption of messages.
     *      - WIFI_PROV_SECURITY_2 SRP6a based authentication and key exchange
     *        + AES-GCM encryption/decryption of messages
     */
    wifi_prov_security_t security = WIFI_PROV_SECURITY_1;

    /* Do we want a proof-of-possession (ignored if Security 0 is selected):
     *      - this should be a string with length > 0
     *      - NULL if not used
     */
    const char *pop = "abcd1234";

    /* This is the structure for passing security parameters
     * for the protocomm security 1.
     */
    wifi_prov_security1_params_t *sec_params = pop;

    const char *username  = NULL;

#elif CONFIG_EXAMPLE_PROV_SECURITY_VERSION_2
    wifi_prov_security_t security = WIFI_PROV_SECURITY_2;
    /* The username must be the same one, which has been used in the generation of salt and verifier */

#if CONFIG_EXAMPLE_PROV_SEC2_DEV_MODE
    /* This pop field represents the password that will be used to generate salt and verifier.
     * The field is present here in order to generate the QR code containing password.
     * In production this password field shall not be stored on the device */
    const char *username  = EXAMPLE_PROV_SEC2_USERNAME;
    const char *pop = EXAMPLE_PROV_SEC2_PWD;
#elif CONFIG_EXAMPLE_PROV_SEC2_PROD_MODE
    /* The username and password shall not be embedded in the firmware,
     * they should be provided to the user by other means.
     * e.g. QR code sticker */
    const char *username  = NULL;
    const char *pop = NULL;
#endif
    /* This is the structure for passing security parameters
     * for the protocomm security 2.
     * If dynamically allocated, sec2_params pointer and its content
     * must be valid till WIFI_PROV_END event is triggered.
     */
    static wifi_prov_security2_params_t sec2_params;

    ESP_RETURN_ON_ERROR(get_sec2_salt(&sec2_params.salt, &sec2_params.salt_len), TAG, "Cannot load Security 2 salt");
    ESP_RETURN_ON_ERROR(get_sec2_verifier(&sec2_params.verifier, &sec2_params.verifier_len), TAG, "Cannot load Security 2 verifier");

    wifi_prov_security2_params_t *sec_params = &sec2_params;
#endif
    /* What is the service key (could be NULL)
     * This translates to :
     *     - Wi-Fi password when scheme is wifi_prov_scheme_softap
     *          (Minimum expected length: 8, maximum 64 for WPA2-PSK)
     *     - simply ignored when scheme is wifi_prov_scheme_ble
     */
    const char *service_key = NULL;

#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_BLE
    /* This step is only useful when scheme is wifi_prov_scheme_ble. This will
     * set a custom 128 bit UUID which will be included in the BLE advertisement
     * and will correspond to the primary GATT service that provides provisioning
     * endpoints as GATT characteristics. Each GATT characteristic will be
     * formed using the primary service UUID as base, with different auto assigned
     * 12th and 13th bytes (assume counting starts from 0th byte). The client side
     * applications must identify the endpoints by reading the User Characteristic
     * Description descriptor (0x2901) for each characteristic, which contains the
     * endpoint name of the characteristic */
    uint8_t custom_service_uuid[] = {
        /* LSB <---------------------------------------
         * ---------------------------------------> MSB */
        0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
        0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02,
    };

    /* If your build fails with linker errors at this point, then you may have
     * forgotten to enable the BT stack or BTDM BLE settings in the SDK (e.g. see
     * the sdkconfig.defaults in the example project) */
    ESP_RETURN_ON_ERROR(wifi_prov_scheme_ble_set_service_uuid(custom_service_uuid), TAG, "Cannot set BLE UUID");
#endif /* CONFIG_EXAMPLE_PROV_TRANSPORT_BLE */

    /* An optional endpoint that applications can create if they expect to
     * get some additional custom data during provisioning workflow.
     * The endpoint name can be anything of your choice.
     * This call must be made before starting the provisioning.
     */
    ESP_RETURN_ON_ERROR(wifi_prov_mgr_endpoint_create("custom-data"), TAG, "Cannot create custom endpoint");

    ESP_LOGI(TAG, "Starting BLE provisioning");
    /* Start provisioning service */
    ESP_RETURN_ON_ERROR(wifi_prov_mgr_start_provisioning(security, (const void *) sec_params, service_name, service_key), TAG, "Cannot start provisioning");

    /* The handler for the optional endpoint created above.
     * This call must be made after starting the provisioning, and only if the endpoint
     * has already been created above.
     */
    ESP_RETURN_ON_ERROR(wifi_prov_mgr_endpoint_register("custom-data", custom_prov_data_handler, NULL), TAG, "Cannot register custom endpoint");

    /* Security material is unchanged; provisioning secrets must not reach logs. */
    (void)username;
    (void)pop;
    ESP_LOGI(TAG, "Waiting for Wi-Fi configuration (service %s)", service_name);
    return ESP_OK;
}


static esp_err_t init_provisioning_manager(void)
{
    wifi_prov_mgr_config_t config = {
#ifdef CONFIG_EXAMPLE_RESET_PROV_MGR_ON_FAILURE
        .wifi_prov_conn_cfg = {
           .wifi_conn_attempts =  CONFIG_EXAMPLE_PROV_MGR_CONNECTION_CNT,
        },
#endif
        /* What is the Provisioning Scheme that we want ?
         * wifi_prov_scheme_softap or wifi_prov_scheme_ble */
#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_BLE
        .scheme = wifi_prov_scheme_ble,
#endif /* CONFIG_EXAMPLE_PROV_TRANSPORT_BLE */
#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_SOFTAP
        .scheme = wifi_prov_scheme_softap,
#endif /* CONFIG_EXAMPLE_PROV_TRANSPORT_SOFTAP */
#ifdef CONFIG_EXAMPLE_PROV_ENABLE_APP_CALLBACK
        .app_event_handler = wifi_prov_event_handler,
#endif /* EXAMPLE_PROV_ENABLE_APP_CALLBACK */

        /* Keep BLE memory reusable. The transport still stops/deinitializes
         * Bluetooth after END; irreversible esp_bt_mem_release is forbidden. */
#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_BLE
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
#endif /* CONFIG_EXAMPLE_PROV_TRANSPORT_BLE */
#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_SOFTAP
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
#endif /* CONFIG_EXAMPLE_PROV_TRANSPORT_SOFTAP */
    };


    esp_err_t err = wifi_prov_mgr_init(config);
    if (err == ESP_OK) {
        xEventGroupSetBits(s_state, MANAGER_BIT);
    }
    return err;
}

static void cleanup_failed_init(void)
{
    while (s_registered_handlers > 0) {
        --s_registered_handlers;
        esp_event_handler_instance_unregister(*s_handlers[s_registered_handlers].base,
                                              ESP_EVENT_ANY_ID,
                                              s_handlers[s_registered_handlers].instance);
    }
    release_provisioning_manager();
    if (s_wifi_initialized) {
        esp_wifi_deinit();
        s_wifi_initialized = false;
    }
    if (s_sta_netif) {
        esp_netif_destroy_default_wifi(s_sta_netif);
        s_sta_netif = NULL;
    }
#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_SOFTAP
    if (s_ap_netif) {
        esp_netif_destroy_default_wifi(s_ap_netif);
        s_ap_netif = NULL;
    }
#endif
    if (s_reconnect_timer) {
        esp_timer_delete(s_reconnect_timer);
        s_reconnect_timer = NULL;
    }
    if (s_state) {
        vEventGroupDelete(s_state);
        s_state = NULL;
    }
    if (s_owns_event_loop) {
        esp_event_loop_delete_default();
        s_owns_event_loop = false;
    }
    /* NVS and global esp_netif state may be shared. Never erase/deinit them. */
}

esp_err_t wifi_manager_init(void)
{
    if (s_init_attempted) {
        return s_init_result;
    }
    s_init_attempted = true;
    esp_err_t ret = nvs_flash_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS initialization failed (%s); stored data was not erased",
                 esp_err_to_name(ret));
        s_init_result = ret;
        return ret;
    }

    ESP_GOTO_ON_ERROR(esp_netif_init(), fail, TAG, "Cannot initialize netif");
    ret = esp_event_loop_create_default();
    if (ret == ESP_OK) {
        s_owns_event_loop = true;
    } else if (ret != ESP_ERR_INVALID_STATE) {
        goto fail;
    }

    s_state = xEventGroupCreate();
    if (!s_state) {
        ret = ESP_ERR_NO_MEM;
        goto fail;
    }
    const esp_timer_create_args_t timer_config = {
        .callback = reconnect_timer_callback,
        .name = "wifi_reconnect",
    };
    ESP_GOTO_ON_ERROR(esp_timer_create(&timer_config, &s_reconnect_timer), fail, TAG,
                      "Cannot create reconnect timer");

    /* Refuse a second owner instead of creating duplicate default netifs. */
    if (esp_netif_get_handle_from_ifkey("WIFI_STA_DEF")) {
        ret = ESP_ERR_INVALID_STATE;
        goto fail;
    }
    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (!s_sta_netif) {
        ret = ESP_ERR_NO_MEM;
        goto fail;
    }
#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_SOFTAP
    if (esp_netif_get_handle_from_ifkey("WIFI_AP_DEF")) {
        ret = ESP_ERR_INVALID_STATE;
        goto fail;
    }
    s_ap_netif = esp_netif_create_default_wifi_ap();
    if (!s_ap_netif) {
        ret = ESP_ERR_NO_MEM;
        goto fail;
    }
#endif
    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_GOTO_ON_ERROR(esp_wifi_init(&wifi_config), fail, TAG, "Cannot initialize Wi-Fi");
    s_wifi_initialized = true;
    ESP_GOTO_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_FLASH), fail, TAG, "Cannot select NVS storage");
    ESP_GOTO_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), fail, TAG, "Cannot set station mode");

    for (; s_registered_handlers < sizeof(s_handlers) / sizeof(s_handlers[0]);
         ++s_registered_handlers) {
        ESP_GOTO_ON_ERROR(esp_event_handler_instance_register(
                              *s_handlers[s_registered_handlers].base, ESP_EVENT_ANY_ID,
                              event_handler, NULL, &s_handlers[s_registered_handlers].instance),
                          fail, TAG, "Cannot register connectivity event handler");
    }
    ESP_GOTO_ON_ERROR(init_provisioning_manager(), fail, TAG, "Cannot initialize provisioning manager");
    bool provisioned = false;
    ESP_GOTO_ON_ERROR(wifi_prov_mgr_is_provisioned(&provisioned), fail, TAG,
                      "Cannot read saved Wi-Fi configuration");
    if (provisioned) {
        xEventGroupSetBits(s_state, PROVISIONED_BIT);
    }
    if (xTaskCreate(control_task, "wifi_control", 6144, NULL, 3, &s_control_task) != pdPASS) {
        ret = ESP_ERR_NO_MEM;
        goto fail;
    }
    s_init_result = ESP_OK;
    return ESP_OK;

fail:
    cleanup_failed_init();
    s_init_result = ret;
    return ret;
}

static esp_err_t start_connectivity(void)
{
    if (wifi_manager_is_provisioned()) {
        ESP_LOGI(TAG, "Saved credentials found; starting Wi-Fi STA without BLE provisioning");
        release_provisioning_manager();
        xEventGroupSetBits(s_state, RECONNECT_BIT);
        s_start_result = esp_wifi_start();
        if (s_start_result != ESP_OK) {
            xEventGroupClearBits(s_state, RECONNECT_BIT);
        }
    } else {
        /* Set before starting: SDK events can arrive before the call returns. */
        xEventGroupSetBits(s_state, PROVISIONING_BIT);
        s_start_result = start_provisioning();
        if (s_start_result != ESP_OK) {
            xEventGroupClearBits(s_state, PROVISIONING_BIT);
            release_provisioning_manager();
            wifi_manager_provisioning_failed_event_t failure = {
                .reason = WIFI_MANAGER_PROV_INTERNAL_ERROR,
                .error = s_start_result,
            };
            publish(WIFI_MANAGER_EVENT_PROVISIONING_FAILED, &failure, sizeof(failure));
        }
    }
    return s_start_result;
}

/* Only this worker starts/deinitializes provisioning after init. It may block
 * in SDK teardown without blocking the default event loop needed by the SDK. */
static esp_err_t reset_credentials(void)
{
    release_provisioning_manager();
    esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED) {
        return err;
    }
    /* deinit can emit END; stop can emit STOP. Drain both before a new manager. */
    xEventGroupClearBits(s_state, DRAINED_BIT);
    ESP_RETURN_ON_ERROR(esp_event_post(WIFI_MANAGER_INTERNAL_EVENT, RESET_DRAINED,
                                      NULL, 0, portMAX_DELAY), TAG, "Cannot drain old events");
    xEventGroupWaitBits(s_state, DRAINED_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    mark_disconnected(0, false);
    xEventGroupClearBits(s_state, STA_RUNNING_BIT | ASSOCIATED_BIT | PROVISIONING_BIT);

    /* reset_provisioning() in IDF 5.5.6 calls esp_wifi_restore(), which resets
     * additional Wi-Fi settings. Persist only an empty STA config instead.
     * The provisioning SDK can switch storage to RAM, so select FLASH explicitly. */
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_FLASH), TAG, "Cannot select persistent storage");
    wifi_config_t empty = {0};
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &empty), TAG, "Cannot clear Wi-Fi credentials");
    xEventGroupClearBits(s_state, PROVISIONED_BIT);
    ESP_LOGI(TAG, "Wi-Fi credentials cleared");
    s_retry_delay_ms = CONFIG_KLIVOX_WIFI_RETRY_INITIAL_MS;
    ESP_RETURN_ON_ERROR(init_provisioning_manager(), TAG, "Cannot recreate provisioning manager");
    xEventGroupSetBits(s_state, PROVISIONING_BIT);
    xEventGroupClearBits(s_state, RESET_BIT);
    err = start_provisioning();
    if (err != ESP_OK) {
        /* Keep stale END from SDK error cleanup away from the next session. */
        xEventGroupSetBits(s_state, RESET_BIT);
        release_provisioning_manager();
        xEventGroupClearBits(s_state, DRAINED_BIT);
        esp_event_post(WIFI_MANAGER_INTERNAL_EVENT, RESET_DRAINED, NULL, 0, portMAX_DELAY);
        xEventGroupWaitBits(s_state, DRAINED_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
        xEventGroupClearBits(s_state, PROVISIONING_BIT);
    }
    return err;
}

static void control_task(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t work;
        xTaskNotifyWait(0, UINT32_MAX, &work, portMAX_DELAY);
        if (work & WORK_START) {
            s_start_result = start_connectivity();
            xEventGroupSetBits(s_state, START_DONE_BIT);
        }
        if (work & WORK_RESET) {
            esp_err_t err = reset_credentials();
            xEventGroupClearBits(s_state, RESET_BIT);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Wi-Fi reset failed: %s; SETUP can be retried", esp_err_to_name(err));
                wifi_manager_provisioning_failed_event_t failure = {
                    .reason = WIFI_MANAGER_PROV_INTERNAL_ERROR, .error = err,
                };
                publish(WIFI_MANAGER_EVENT_PROVISIONING_FAILED, &failure, sizeof(failure));
            }
            portENTER_CRITICAL(&s_request_lock);
            s_reset_pending = false;
            portEXIT_CRITICAL(&s_request_lock);
        } else if (work & WORK_END) {
            release_provisioning_manager();
            ESP_LOGI(TAG, "Provisioning resources released (BLE memory reusable)");
            esp_event_post(WIFI_MANAGER_INTERNAL_EVENT, PROVISIONING_RELEASED, NULL, 0, portMAX_DELAY);
        }
    }
}

esp_err_t wifi_manager_start(void)
{
    if (s_init_result != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_start_attempted) {
        s_start_attempted = true;
        xTaskNotify(s_control_task, WORK_START, eSetBits);
    }
    xEventGroupWaitBits(s_state, START_DONE_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    return s_start_result;
}

esp_err_t wifi_manager_reset_credentials(void)
{
    if (s_init_result != ESP_OK || !s_start_attempted ||
        !(xEventGroupGetBits(s_state) & START_DONE_BIT)) {
        return ESP_ERR_INVALID_STATE;
    }
    portENTER_CRITICAL(&s_request_lock);
    bool busy = s_reset_pending;
    s_reset_pending = true;
    portEXIT_CRITICAL(&s_request_lock);
    if (busy) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = esp_event_post(WIFI_MANAGER_INTERNAL_EVENT, RESET_REQUEST, NULL, 0, 0);
    if (err != ESP_OK) {
        portENTER_CRITICAL(&s_request_lock);
        s_reset_pending = false;
        portEXIT_CRITICAL(&s_request_lock);
    }
    return err;
}

bool wifi_manager_is_connected(void)
{
    return s_state && (xEventGroupGetBits(s_state) & CONNECTED_BIT) != 0;
}

bool wifi_manager_is_provisioned(void)
{
    return s_state && (xEventGroupGetBits(s_state) & PROVISIONED_BIT) != 0;
}
