#include "app_setup.h"

#include "app_feedback.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "setup_button_manager.h"
#include "wifi_manager.h"

static void setup_long_press(void)
{
    app_feedback_on_setup();
    ESP_LOGI("KLIVOX_SETUP", "Requesting Wi-Fi credentials reset");
    const esp_err_t err = wifi_manager_reset_credentials();
    if (err != ESP_OK) {
        ESP_LOGW("KLIVOX_SETUP", "Reset request rejected: %s", esp_err_to_name(err));
    }
}

esp_err_t app_setup_init(void)
{
    const int pin = KLIVOX_SETUP_GPIO;
    if (pin == CONFIG_KLIVOX_BUZZER_GPIO || pin == CONFIG_KLIVOX_TEMPERATURE_GPIO ||
        pin == CONFIG_KLIVOX_IR_RX_GPIO || pin == CONFIG_KLIVOX_CONTACT_1_GPIO ||
        pin == CONFIG_KLIVOX_CONTACT_2_GPIO || pin == CONFIG_KLIVOX_BUTTON_POWER_GPIO) {
        return ESP_ERR_INVALID_ARG;
    }
    const setup_button_manager_config_t config = {
        .gpio_num = KLIVOX_SETUP_GPIO,
        .hold_ms = KLIVOX_SETUP_HOLD_MS,
        .debounce_ms = KLIVOX_SETUP_DEBOUNCE_MS,
        .on_long_press = setup_long_press,
    };
    return setup_button_manager_init(&config);
}
