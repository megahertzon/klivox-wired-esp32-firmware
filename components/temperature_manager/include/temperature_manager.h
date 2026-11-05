#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float temperature_c;
    uint64_t sensor_address; /* 1-Wire ROM; identifies the detected DS18B20. */
    int64_t timestamp_us;    /* Monotonic time since boot, from esp_timer. */
} temperature_manager_reading_t;

/** Runs in the sensor task, once per attempt. reading is NULL on error.
 * Copy the reading if retaining it. Keep this callback short; notify/queue work
 * for MQTT or Zigbee rather than calling those stacks directly from this task.
 */
typedef void (*temperature_manager_callback_t)(
    esp_err_t status, const temperature_manager_reading_t *reading, void *user_ctx);

typedef struct {
    int gpio_num;
    uint32_t sample_period_ms; /* 1000 .. 3600000 */
    temperature_manager_callback_t on_sample; /* Optional. */
    void *user_ctx;
} temperature_manager_config_t;

/** Initialize a dedicated RMT 1-Wire bus for ONE externally powered DS18B20.
 * Call init/start serially from the startup task. The config is copied; user_ctx
 * must remain valid for the module's lifetime. No network/event-loop dependency.
 * Sensor discovery runs in the worker, so an absent sensor does not fail init.
 * Reinitialization is rejected. Resources live for the duration of this boot.
 */
esp_err_t temperature_manager_init(const temperature_manager_config_t *config);

/** Start periodic discovery, conversion and logging in a FreeRTOS task.
 * Repeated successful starts are harmless. No sensor/CRC error aborts the app;
 * errors invalidate the latest result and discovery retries on the next cycle.
 */
esp_err_t temperature_manager_start(void);

/** Thread-safe, immediate snapshot. out_reading is written only on ESP_OK.
 * Returns ESP_ERR_INVALID_STATE before init/first sample, otherwise the last
 * measurement error or ESP_OK. Check timestamp_us for freshness; this function
 * does not initiate a conversion. Callers must not access the bus directly.
 */
esp_err_t temperature_manager_get_latest(temperature_manager_reading_t *out_reading);

#ifdef __cplusplus
}
#endif
