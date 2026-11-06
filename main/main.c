#include "esp_log.h"
#include "app_feedback.h"
#include "ir_receiver.h"
#include "sdkconfig.h"
#include "temperature_manager.h"
#include "wifi_manager.h"

void app_main(void)
{
    /* Establish the inactive buzzer level before radios and other drivers. */
    const esp_err_t feedback_err = app_feedback_init();
    ESP_LOGI("KLIVOX", "Starting Klivox Wired");
    if (feedback_err != ESP_OK) {
        ESP_LOGE("KLIVOX", "Buzzer initialization failed: %s", esp_err_to_name(feedback_err));
    }

    /* Allocate the IR library's capture buffers before the radios use heap.
     * Capture itself starts below, after the other modules initialize. */
    const ir_receiver_config_t ir_config = {
        .gpio_num = CONFIG_KLIVOX_IR_RX_GPIO,
        .capture_buffer_entries = CONFIG_KLIVOX_IR_RX_BUFFER_SIZE,
        .frame_timeout_ms = CONFIG_KLIVOX_IR_RX_TIMEOUT_MS,
        .on_frame = app_feedback_on_ir_frame,
#ifdef CONFIG_KLIVOX_IR_DUMP_RAW
        .dump_raw = true,
#endif
    };
    esp_err_t ir_err = ir_config.gpio_num == CONFIG_KLIVOX_TEMPERATURE_GPIO
                       ? ESP_ERR_INVALID_ARG : ir_receiver_init(&ir_config);

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

    if (ir_err == ESP_OK) {
        ir_err = ir_receiver_start();
    }
    if (ir_err != ESP_OK) {
        ESP_LOGE("KLIVOX", "IR initialization failed: %s (check GPIO conflict/heap)", esp_err_to_name(ir_err));
    }

    /* Connectivity, temperature and IR reception continue independently. */
}
