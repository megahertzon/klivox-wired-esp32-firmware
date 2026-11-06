#include "app_status.h"

#include "esp_log.h"
#include "sdkconfig.h"
#include "setup_button_manager.h"
#include "status_led_manager.h"
#include "wifi_manager.h"

static bool s_ready;
static bool s_registered;
static bool s_connectivity_error;

void app_status_on_temperature_sample(esp_err_t status,
                                     const temperature_manager_reading_t *reading,
                                     void *user_ctx)
{
    (void)reading;
    (void)user_ctx;
    const char *state = s_ready
                        ? status_led_manager_state_name(status_led_manager_get_state())
                        : "NOT_INITIALIZED";
    ESP_LOGI("KLIVOX_LED", "State at temperature sample: %s (sensor=%s)",
             state, esp_err_to_name(status));
}

esp_err_t app_status_init(void)
{
    const int led_pins[] = {CONFIG_KLIVOX_LED_RED_GPIO,
                            CONFIG_KLIVOX_LED_GREEN_GPIO,
                            CONFIG_KLIVOX_LED_BLUE_GPIO};
    const int occupied[] = {KLIVOX_SETUP_GPIO, CONFIG_KLIVOX_BUZZER_GPIO,
                            CONFIG_KLIVOX_TEMPERATURE_GPIO, CONFIG_KLIVOX_IR_RX_GPIO,
                            CONFIG_KLIVOX_CONTACT_1_GPIO, CONFIG_KLIVOX_CONTACT_2_GPIO,
                            CONFIG_KLIVOX_BUTTON_POWER_GPIO};
    for (unsigned i = 0; i < sizeof(led_pins) / sizeof(led_pins[0]); ++i) {
        for (unsigned j = 0; j < sizeof(occupied) / sizeof(occupied[0]); ++j) {
            if (led_pins[i] == occupied[j]) {
                ESP_LOGE("KLIVOX_LED", "GPIO%d conflicts with another peripheral", led_pins[i]);
                return ESP_ERR_INVALID_ARG;
            }
        }
    }
    esp_err_t err = status_led_manager_init();
    s_ready = err == ESP_OK;
    return err;
}

static void connectivity_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    bool provisioning = wifi_manager_is_provisioning();
    bool connected = wifi_manager_is_connected();
    if (id == WIFI_MANAGER_EVENT_PROVISIONING_FAILED && data) {
        const wifi_manager_provisioning_failed_event_t *failure = data;
        if (failure->reason == WIFI_MANAGER_PROV_INTERNAL_ERROR) {
            s_connectivity_error = true;
            (void)status_led_manager_set_condition(STATUS_LED_ERROR, true);
        }
    } else if (s_connectivity_error &&
               (id == WIFI_MANAGER_EVENT_CONNECTED || id == WIFI_MANAGER_EVENT_PROVISIONING_STARTED)) {
        s_connectivity_error = false;
        (void)status_led_manager_set_condition(STATUS_LED_ERROR, false);
    }
    status_led_state_t state = connected ? STATUS_LED_WIFI_CONNECTED : STATUS_LED_WIFI_CONNECTING;
    if (!connected && (id == WIFI_MANAGER_EVENT_DISCONNECTED ||
                       id == WIFI_MANAGER_EVENT_PROVISIONING_FAILED)) {
        state = STATUS_LED_OFFLINE;
    }
    (void)status_led_manager_set_connectivity(state, provisioning);
}

esp_err_t app_status_register_connectivity(void)
{
    if (!s_ready) return ESP_ERR_INVALID_STATE;
    if (s_registered) return ESP_OK;
    esp_err_t err = esp_event_handler_register(WIFI_MANAGER_EVENT, ESP_EVENT_ANY_ID,
                                              connectivity_event, NULL);
    if (err == ESP_OK) {
        s_registered = true;
        (void)status_led_manager_set_connectivity(STATUS_LED_WIFI_CONNECTING, false);
    }
    return err;
}
