#pragma once

#include "esp_err.h"

/* Indices used by digital_input_state_t.input_index in this application. */
enum {
    APP_INPUT_CONTACT_1,
    APP_INPUT_CONTACT_2,
    APP_INPUT_BUTTON_POWER,
    APP_INPUT_COUNT
};

esp_err_t app_inputs_init(void);
esp_err_t app_inputs_start(void);
