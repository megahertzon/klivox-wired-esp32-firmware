#include "buzzer.h"
#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "buzzer";
static int s_gpio = -1;
static esp_timer_handle_t s_off_timer;
static SemaphoreHandle_t s_idle;
static uint32_t s_duration_ms;
static uint32_t s_gap_ms;
static unsigned s_remaining;
static bool s_sounding;

static void end_pattern(void)
{
    (void)gpio_set_level(s_gpio, 0);
    xSemaphoreGive(s_idle);
}

static void buzzer_off(void *arg)
{
    (void)arg;
    esp_err_t err;
    if (s_sounding) {
        err = gpio_set_level(s_gpio, 0);
        s_sounding = false;
        if (--s_remaining == 0 || err != ESP_OK) {
            end_pattern();
            return;
        }
        err = esp_timer_start_once(s_off_timer, s_gap_ms * 1000ULL);
    } else {
        err = gpio_set_level(s_gpio, 1);
        s_sounding = true;
        if (err == ESP_OK) {
            err = esp_timer_start_once(s_off_timer, s_duration_ms * 1000ULL);
        }
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Cannot advance buzzer pattern: %s", esp_err_to_name(err));
        end_pattern();
    }
}

esp_err_t buzzer_init(const buzzer_config_t *config)
{
    if (!config || !GPIO_IS_VALID_OUTPUT_GPIO(config->gpio_num)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_off_timer) {
        return config->gpio_num == s_gpio ? ESP_OK : ESP_ERR_INVALID_STATE;
    }
    const int pin = config->gpio_num;
    esp_err_t err = gpio_set_level(pin, 0);
    if (err != ESP_OK) {
        return err;
    }
    const gpio_config_t pin_config = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&pin_config);
    if (err == ESP_OK) {
        err = gpio_set_level(pin, 0);
    }
    if (err != ESP_OK) {
        return err;
    }
    SemaphoreHandle_t idle = xSemaphoreCreateBinary();
    if (!idle) {
        return ESP_ERR_NO_MEM;
    }
    const esp_timer_create_args_t timer_config = {
        .callback = buzzer_off,
        .name = "buzzer_off",
    };
    esp_timer_handle_t timer = NULL;
    err = esp_timer_create(&timer_config, &timer);
    if (err != ESP_OK) {
        vSemaphoreDelete(idle);
        return err;
    }
    s_gpio = pin;
    s_idle = idle;
    s_off_timer = timer;
    xSemaphoreGive(s_idle);
    ESP_LOGI(TAG, "Active buzzer ready on GPIO%d; idle LOW", s_gpio);
    return ESP_OK;
}

esp_err_t buzzer_beep(uint32_t duration_ms)
{
    return buzzer_beep_pattern(1, duration_ms, 0);
}

esp_err_t buzzer_beep_pattern(unsigned count, uint32_t duration_ms, uint32_t gap_ms)
{
    if (count < 1 || count > 8 || duration_ms < 50 || duration_ms > 1000 ||
        (count > 1 && (gap_ms < 50 || gap_ms > 1000))) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_off_timer) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_idle, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    s_remaining = count;
    s_duration_ms = duration_ms;
    s_gap_ms = gap_ms;
    s_sounding = true;
    esp_err_t err = gpio_set_level(s_gpio, 1);
    if (err == ESP_OK) {
        err = esp_timer_start_once(s_off_timer, duration_ms * 1000ULL);
    }
    if (err != ESP_OK) {
        // Never leave the transistor ON if arming the off timer failed.
        (void)gpio_set_level(s_gpio, 0);
        xSemaphoreGive(s_idle);
    }
    return err;
}
