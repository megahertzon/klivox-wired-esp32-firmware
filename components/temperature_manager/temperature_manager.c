#include "temperature_manager.h"

#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "ds18b20.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "onewire_bus.h"
#include "onewire_device.h"

static const char *TAG = "temperature_manager";
static temperature_manager_config_t s_config;
static onewire_bus_handle_t s_bus;
/* Only the sensor task touches the sensor and bus after initialization. */
static ds18b20_device_handle_t s_sensor;
static uint64_t s_sensor_address;
static TaskHandle_t s_task;
static portMUX_TYPE s_reading_lock = portMUX_INITIALIZER_UNLOCKED;
static temperature_manager_reading_t s_latest;
static esp_err_t s_latest_status = ESP_ERR_INVALID_STATE;

static esp_err_t discover_sensor(void)
{
    onewire_device_iter_handle_t iterator = NULL;
    esp_err_t err = onewire_new_device_iter(s_bus, &iterator);
    if (err != ESP_OK) {
        return err;
    }

    onewire_device_t device;
    while ((err = onewire_device_iter_get_next(iterator, &device)) == ESP_OK) {
        if ((device.address & 0xff) != 0x28) {
            continue;
        }
        const ds18b20_config_t config = {};
        err = ds18b20_new_device_from_enumeration(&device, &config, &s_sensor);
        if (err == ESP_OK) {
            s_sensor_address = device.address;
            ESP_LOGI(TAG, "DS18B20 detected on GPIO%d, ROM=%016" PRIX64,
                     s_config.gpio_num, s_sensor_address);
        }
        /* This component deliberately supports one sensor per dedicated bus. */
        break;
    }

    esp_err_t cleanup_err = onewire_del_device_iter(iterator);
    if (cleanup_err != ESP_OK) {
        ESP_LOGE(TAG, "Cannot release device iterator: %s", esp_err_to_name(cleanup_err));
    }
    return err;
}

static esp_err_t take_reading(temperature_manager_reading_t *reading)
{
    esp_err_t err = ESP_OK;
    if (!s_sensor) {
        err = discover_sensor();
    }
    if (err == ESP_OK) {
        /* The official driver yields this task for up to 800 ms, allowing a
         * full 12-bit conversion. Neither app_main nor Wi-Fi/BLE waits here. */
        err = ds18b20_trigger_temperature_conversion(s_sensor);
    }
    float temperature_c = 0;
    if (err == ESP_OK) {
        /* The driver verifies the scratchpad CRC and rejects its known
         * uninitialized power-on pattern. Do not publish fabricated defaults. */
        err = ds18b20_get_temperature(s_sensor, &temperature_c);
    }
    if (err == ESP_OK && (!isfinite(temperature_c) ||
                         temperature_c < -55.0f || temperature_c > 125.0f)) {
        err = ESP_ERR_INVALID_RESPONSE;
    }
    if (err != ESP_OK) {
        if (s_sensor) {
            ds18b20_del_device(s_sensor);
            s_sensor = NULL;
        }
        return err;
    }
    *reading = (temperature_manager_reading_t) {
        .temperature_c = temperature_c,
        .sensor_address = s_sensor_address,
        .timestamp_us = esp_timer_get_time(),
    };
    return ESP_OK;
}

static void sensor_task(void *arg)
{
    const TickType_t period = pdMS_TO_TICKS(s_config.sample_period_ms);
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        temperature_manager_reading_t reading = {0};
        const esp_err_t err = take_reading(&reading);

        portENTER_CRITICAL(&s_reading_lock);
        s_latest_status = err;
        if (err == ESP_OK) {
            s_latest = reading;
        }
        portEXIT_CRITICAL(&s_reading_lock);

        if (err == ESP_OK) {
            ESP_LOGI(TAG, "DS18B20: %.2f C", (double)reading.temperature_c);
        } else {
            ESP_LOGW(TAG, "DS18B20 reading failed: %s; check sensor/pull-up on GPIO%d; retry on next %lu s cycle",
                     esp_err_to_name(err), s_config.gpio_num,
                     (unsigned long)(s_config.sample_period_ms / 1000));
        }
        if (s_config.on_sample) {
            s_config.on_sample(err, err == ESP_OK ? &reading : NULL, s_config.user_ctx);
        }

        /* Avoid immediate catch-up attempts if a bus timeout/callback overran
         * the period. Normal conversions retain a stable start-to-start period. */
        if ((TickType_t)(xTaskGetTickCount() - last_wake) >= period) {
            last_wake = xTaskGetTickCount();
        }
        xTaskDelayUntil(&last_wake, period);
    }
}

esp_err_t temperature_manager_init(const temperature_manager_config_t *config)
{
    if (!config || !GPIO_IS_VALID_OUTPUT_GPIO(config->gpio_num) ||
        config->sample_period_ms < 1000 || config->sample_period_ms > 3600000) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_bus) {
        return ESP_ERR_INVALID_STATE;
    }

    const onewire_bus_config_t bus_config = {
        .bus_gpio_num = config->gpio_num,
        /* An external 4.7 kOhm pull-up to 3.3 V is required. */
        .flags.en_pull_up = false,
    };
    const onewire_bus_rmt_config_t rmt_config = { .max_rx_bytes = 10 };
    esp_err_t err = onewire_new_bus_rmt(&bus_config, &rmt_config, &s_bus);
    if (err != ESP_OK) {
        return err;
    }
    s_config = *config;
    ESP_LOGI(TAG, "1-Wire ready on GPIO%d; sampling period %lu ms",
             s_config.gpio_num, (unsigned long)s_config.sample_period_ms);
    return ESP_OK;
}

esp_err_t temperature_manager_start(void)
{
    if (!s_bus) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_task) {
        return ESP_OK;
    }
    if (xTaskCreate(sensor_task, "temperature", 4096, NULL, 4, &s_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t temperature_manager_get_latest(temperature_manager_reading_t *out_reading)
{
    if (!out_reading) {
        return ESP_ERR_INVALID_ARG;
    }
    portENTER_CRITICAL(&s_reading_lock);
    const esp_err_t status = s_latest_status;
    if (status == ESP_OK) {
        *out_reading = s_latest;
    }
    portEXIT_CRITICAL(&s_reading_lock);
    return status;
}
