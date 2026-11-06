#pragma once

#include <stdint.h>
#include "driver/gpio.h"
#include "esp_err.h"

#ifndef KLIVOX_SETUP_GPIO
#define KLIVOX_SETUP_GPIO GPIO_NUM_23
#endif
#ifndef KLIVOX_SETUP_HOLD_MS
#define KLIVOX_SETUP_HOLD_MS 5000
#endif
#ifndef KLIVOX_SETUP_DEBOUNCE_MS
#define KLIVOX_SETUP_DEBOUNCE_MS 50
#endif

/** Runs once per long press in the button task. Must return promptly. */
typedef void (*setup_button_long_press_cb_t)(void);

typedef struct {
    gpio_num_t gpio_num;
    uint32_t hold_ms;
    uint32_t debounce_ms;
    setup_button_long_press_cb_t on_long_press;
} setup_button_manager_config_t;

/** Call from one startup task. Copies configuration; board checks belong to app.
 * Active LOW with internal pull-up. ISR service is shared and never uninstalled.
 * No Wi-Fi, NVS or buzzer dependency. Resources live for this boot.
 */
esp_err_t setup_button_manager_init(const setup_button_manager_config_t *config);
