#include "esp_log.h"
#include "sdkconfig.h"
#include "temperature_manager.h"
#include "wifi_manager.h"

void app_main(void)
{
    ESP_LOGI("KLIVOX", "Starting Klivox Wired");

    esp_err_t err = wifi_manager_init();
    if (err == ESP_OK) {
        /* Future modules can register WIFI_MANAGER_EVENT handlers here. */
        err = wifi_manager_start();
    }
    if (err != ESP_OK) {
        ESP_LOGE("KLIVOX", "Connectivity initialization failed: %s", esp_err_to_name(err));
    }

    const temperature_manager_config_t temperature_config = {
        .gpio_num = CONFIG_KLIVOX_TEMPERATURE_GPIO,
        .sample_period_ms = CONFIG_KLIVOX_TEMPERATURE_PERIOD_SECONDS * 1000U,
    };
    err = temperature_manager_init(&temperature_config);
    if (err == ESP_OK) {
        err = temperature_manager_start();
    }
    if (err != ESP_OK) {
        ESP_LOGE("KLIVOX", "Temperature sensor initialization failed: %s", esp_err_to_name(err));
    }

    /* Connectivity and temperature acquisition continue in their own tasks. */
}
