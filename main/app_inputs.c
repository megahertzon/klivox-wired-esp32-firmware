#include "app_inputs.h"

#include "digital_inputs.h"
#include "esp_log.h"
#include "sdkconfig.h"

static void input_changed(const digital_input_state_t *state, void *ctx)
{
    (void)ctx;
    const char *initial = state->initial ? " [inicial]" : "";
    if (state->input_index <= APP_INPUT_CONTACT_2) {
        ESP_LOGI("app_inputs", "CONTACTO %u GPIO%d: %s (%s)%s",
                 state->input_index + 1, state->gpio_num,
                 state->active ? "ABIERTO" : "CERRADO",
                 state->level_high ? "HIGH" : "LOW", initial);
        return;
    }
    if (state->input_index != APP_INPUT_BUTTON_POWER) {
        return;
    }
    ESP_LOGI("app_inputs", "PULSADOR ENCENDIDO/APAGADO GPIO%d: %s (%s)%s",
             state->gpio_num, state->active ? "PULSADO" : "LIBERADO",
             state->level_high ? "HIGH" : "LOW", initial);
    if (state->active && !state->initial) {
        ESP_LOGI("app_inputs", "ACCION SOLICITADA: POWER_TOGGLE");
    }
}

esp_err_t app_inputs_init(void)
{
    const digital_inputs_config_t config = {
        .input_count = APP_INPUT_COUNT,
        .inputs = {
            [APP_INPUT_CONTACT_1] = {CONFIG_KLIVOX_CONTACT_1_GPIO, 1, CONFIG_KLIVOX_CONTACT_DEBOUNCE_MS},
            [APP_INPUT_CONTACT_2] = {CONFIG_KLIVOX_CONTACT_2_GPIO, 1, CONFIG_KLIVOX_CONTACT_DEBOUNCE_MS},
            [APP_INPUT_BUTTON_POWER] = {CONFIG_KLIVOX_BUTTON_POWER_GPIO, 0, CONFIG_KLIVOX_BUTTON_DEBOUNCE_MS},
        },
        .on_change = input_changed,
    };
    for (unsigned i = 0; i < config.input_count; ++i) {
        const int pin = config.inputs[i].gpio_num;
        if (pin == CONFIG_KLIVOX_IR_RX_GPIO || pin == CONFIG_KLIVOX_TEMPERATURE_GPIO ||
            pin == CONFIG_KLIVOX_BUZZER_GPIO) {
            ESP_LOGE("app_inputs", "Input %u GPIO%d conflicts with another component", i, pin);
            return ESP_ERR_INVALID_ARG;
        }
    }
    /* The reusable driver validates duplicate and invalid GPIOs before setup. */
    return digital_inputs_init(&config);
}

esp_err_t app_inputs_start(void)
{
    return digital_inputs_start();
}
