#include "app_feedback.h"

#include <stdbool.h>
#include <stdatomic.h>
#include "esp_timer.h"
#include "buzzer.h"
#include "esp_log.h"
#include "sdkconfig.h"

static bool s_ready;
static esp_timer_handle_t s_setup_timer;
static atomic_bool s_setup_pending;

static void setup_feedback(void *arg)
{
    (void)arg;
    esp_err_t err = buzzer_beep_pattern(2, 100, 100);
    if (err == ESP_ERR_TIMEOUT) {
        /* Finish any IR pulse already playing, then reserve both SETUP beeps. */
        err = esp_timer_start_once(s_setup_timer, 10000);
        if (err == ESP_OK) {
            return;
        }
    }
    atomic_store(&s_setup_pending, false);
    if (err != ESP_OK) {
        ESP_LOGW("app_feedback", "Cannot request SETUP beeps: %s", esp_err_to_name(err));
    }
}

void app_feedback_on_setup(void)
{
    if (!s_ready || atomic_exchange(&s_setup_pending, true)) {
        return;
    }
    /* Keep buzzer work out of the SETUP callback; reset proceeds immediately. */
    const esp_err_t err = esp_timer_start_once(s_setup_timer, 1);
    if (err != ESP_OK) {
        atomic_store(&s_setup_pending, false);
        ESP_LOGW("app_feedback", "Cannot schedule SETUP beeps: %s", esp_err_to_name(err));
    }
}

esp_err_t app_feedback_init(void)
{
    if (CONFIG_KLIVOX_BUZZER_GPIO == CONFIG_KLIVOX_IR_RX_GPIO ||
        CONFIG_KLIVOX_BUZZER_GPIO == CONFIG_KLIVOX_TEMPERATURE_GPIO ||
        CONFIG_KLIVOX_BUZZER_GPIO == CONFIG_KLIVOX_CONTACT_1_GPIO ||
        CONFIG_KLIVOX_BUZZER_GPIO == CONFIG_KLIVOX_CONTACT_2_GPIO ||
        CONFIG_KLIVOX_BUZZER_GPIO == CONFIG_KLIVOX_BUTTON_POWER_GPIO) {
        ESP_LOGE("app_feedback", "Buzzer GPIO conflicts with an input");
        return ESP_ERR_INVALID_ARG;
    }
    const buzzer_config_t config = { .gpio_num = CONFIG_KLIVOX_BUZZER_GPIO };
    const esp_err_t err = buzzer_init(&config);
    if (err != ESP_OK) {
        return err;
    }
    if (!s_setup_timer) {
        const esp_timer_create_args_t timer = {
            .callback = setup_feedback,
            .name = "setup_feedback",
        };
        esp_err_t timer_err = esp_timer_create(&timer, &s_setup_timer);
        if (timer_err != ESP_OK) {
            return timer_err;
        }
    }
    s_ready = true;
    return ESP_OK;
}

void app_feedback_on_ir_frame(const ir_receiver_frame_t *frame,
                              const ir_receiver_raw_view_t *raw, void *user_ctx)
{
    (void)raw;
    (void)user_ctx;
    // Feedback acknowledges reception, not protocol support or appliance state.
    if (!s_ready || atomic_load(&s_setup_pending) || !frame || frame->overflow || !frame->raw_count) {
        return;
    }
    const esp_err_t err = buzzer_beep(CONFIG_KLIVOX_BUZZER_PULSE_MS);
    if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
        ESP_LOGW("app_feedback", "Cannot request IR beep: %s", esp_err_to_name(err));
    }
}
