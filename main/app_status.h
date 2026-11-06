#pragma once
#include "esp_err.h"
#include "temperature_manager.h"

esp_err_t app_status_init(void);
/* Register after wifi_manager_init, before wifi_manager_start. */
esp_err_t app_status_register_connectivity(void);
/* Logs the LED snapshot after each sensor attempt, without changing its state. */
void app_status_on_temperature_sample(esp_err_t status,
                                     const temperature_manager_reading_t *reading,
                                     void *user_ctx);
