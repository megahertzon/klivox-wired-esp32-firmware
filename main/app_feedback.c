#include "app_feedback.h"

#include <stdbool.h>
#include "buzzer.h"
#include "esp_log.h"
#include "sdkconfig.h"

static bool s_ready;

esp_err_t app_feedback_init(void)
{
    if (CONFIG_KLIVOX_BUZZER_GPIO == CONFIG_KLIVOX_IR_RX_GPIO ||
        CONFIG_KLIVOX_BUZZER_GPIO == CONFIG_KLIVOX_TEMPERATURE_GPIO) {
        ESP_LOGE("app_feedback", "Buzzer GPIO conflicts with IR or DS18B20");
        return ESP_ERR_INVALID_ARG;
    }
    const buzzer_config_t config = { .gpio_num = CONFIG_KLIVOX_BUZZER_GPIO };
    const esp_err_t err = buzzer_init(&config);
    s_ready = err == ESP_OK;
    return err;
}

void app_feedback_on_ir_frame(const ir_receiver_frame_t *frame,
                              const ir_receiver_raw_view_t *raw, void *user_ctx)
{
    (void)raw;
    (void)user_ctx;
    // Feedback acknowledges reception, not protocol support or appliance state.
    if (!s_ready || !frame || frame->overflow || !frame->raw_count) {
        return;
    }
    const esp_err_t err = buzzer_beep(CONFIG_KLIVOX_BUZZER_PULSE_MS);
    if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
        ESP_LOGW("app_feedback", "Cannot request IR beep: %s", esp_err_to_name(err));
    }
}
