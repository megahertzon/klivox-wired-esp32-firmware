#include "esp_log.h"
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
        return;
    }

    /* Initialize future modules here; connectivity continues asynchronously. */
}
