
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_chip_info.h"

static const char *TAG = "KLIVOX";

void app_main(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    ESP_LOGI(TAG, "==========================");
    ESP_LOGI(TAG, "    KLIVOX WIRED v0.1");
    ESP_LOGI(TAG, "==========================");

    ESP_LOGI(TAG, "Nucleos CPU: %d", chip_info.cores);
    ESP_LOGI(TAG, "Revision chip: %d", chip_info.revision);

    while (1) {
        ESP_LOGI(TAG, "Klivox funcionando correctamente");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
