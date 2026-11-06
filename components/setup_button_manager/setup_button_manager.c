#include "setup_button_manager.h"

#include <stdbool.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "KLIVOX_SETUP";
static TaskHandle_t s_task;
static SemaphoreHandle_t s_ready;
static esp_err_t s_start_result;
static setup_button_manager_config_t s_config;
static portMUX_TYPE s_edge_lock = portMUX_INITIALIZER_UNLOCKED;
static int64_t s_last_edge_us;
static int s_level;

/* Shared GPIO service uses flags=0, like digital_inputs / IR. No radio or NVS
 * operations in this ISR. Every edge restarts the continuous-level interval,
 * even if both edges of a short pulse occur before the task gets CPU time. */
static void setup_edge(void *arg)
{
    (void)arg;
    portENTER_CRITICAL_ISR(&s_edge_lock);
    s_level = gpio_get_level(s_config.gpio_num);
    s_last_edge_us = esp_timer_get_time();
    portEXIT_CRITICAL_ISR(&s_edge_lock);
    BaseType_t wake = pdFALSE;
    vTaskNotifyGiveFromISR(s_task, &wake);
    if (wake) {
        portYIELD_FROM_ISR();
    }
}

static TickType_t wait_ticks(int64_t us)
{
    /* Round up, including when the FreeRTOS tick is coarser than debounce. */
    uint64_t ms = (us + 999) / 1000;
    return (ms + portTICK_PERIOD_MS - 1) / portTICK_PERIOD_MS;
}

static void button_task(void *arg)
{
    (void)arg;
    esp_err_t err = gpio_install_isr_service(0);
    if (err == ESP_ERR_INVALID_STATE) {
        err = ESP_OK;
    }
    if (err == ESP_OK) {
        portENTER_CRITICAL(&s_edge_lock);
        s_level = gpio_get_level(s_config.gpio_num);
        s_last_edge_us = esp_timer_get_time();
        portEXIT_CRITICAL(&s_edge_lock);
        err = gpio_isr_handler_add(s_config.gpio_num, setup_edge, NULL);
    }
    s_start_result = err;
    xSemaphoreGive(s_ready);
    if (err != ESP_OK) {
        vTaskDelete(NULL);
        return;
    }

    bool fired = false;
    bool press_logged = false;
    const int64_t debounce_us = (int64_t)s_config.debounce_ms * 1000;
    const int64_t hold_us = (int64_t)s_config.hold_ms * 1000;
    for (;;) {
        portENTER_CRITICAL(&s_edge_lock);
        int level = s_level;
        int64_t edge_us = s_last_edge_us;
        portEXIT_CRITICAL(&s_edge_lock);
        int64_t elapsed = esp_timer_get_time() - edge_us;
        TickType_t wait = portMAX_DELAY;
        if (elapsed < debounce_us) {
            wait = wait_ticks(debounce_us - elapsed);
        } else if (level != 0) {
            /* Only a debounced release rearms a held button. */
            fired = false;
            press_logged = false;
        } else if (!fired) {
            if (!press_logged) {
                ESP_LOGI(TAG, "SETUP button pressed");
                press_logged = true;
            }
            if (elapsed >= hold_us) {
                /* Recheck under the ISR lock before committing the event. */
                portENTER_CRITICAL(&s_edge_lock);
                bool continuous = s_level == 0 && s_last_edge_us == edge_us &&
                                  gpio_get_level(s_config.gpio_num) == 0;
                portEXIT_CRITICAL(&s_edge_lock);
                if (!continuous) {
                    continue;
                }
                fired = true;
                ESP_LOGI(TAG, "Long press detected (%lu ms)", (unsigned long)s_config.hold_ms);
                s_config.on_long_press();
            } else {
                wait = wait_ticks(hold_us - elapsed);
            }
        }
        ulTaskNotifyTake(pdTRUE, wait);
    }
}

esp_err_t setup_button_manager_init(const setup_button_manager_config_t *config)
{
    if (s_task) {
        return s_start_result;
    }
    /* Board ownership belongs to the application; validate electrical/timing limits. */
    if (!config || !GPIO_IS_VALID_GPIO(config->gpio_num) || config->gpio_num >= GPIO_NUM_34 ||
        !config->on_long_press || config->debounce_ms < 10 || config->debounce_ms > 1000 ||
        config->hold_ms < config->debounce_ms || config->hold_ms > 60000) {
        return ESP_ERR_INVALID_ARG;
    }
    s_config = *config;
    s_ready = xSemaphoreCreateBinary();
    if (!s_ready) {
        return ESP_ERR_NO_MEM;
    }
    const gpio_config_t pin_config = {
        .pin_bit_mask = 1ULL << s_config.gpio_num,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    esp_err_t err = gpio_config(&pin_config);
#if CONFIG_FREERTOS_NUMBER_OF_CORES > 1
    const BaseType_t core = 1;
#else
    const BaseType_t core = 0;
#endif
    if (err == ESP_OK) {
        if (xTaskCreatePinnedToCore(button_task, "setup_button", 3072, NULL, 1,
                                    &s_task, core) != pdPASS) {
            err = ESP_ERR_NO_MEM;
        } else {
            xSemaphoreTake(s_ready, portMAX_DELAY);
            err = s_start_result;
        }
    }
    vSemaphoreDelete(s_ready);
    s_ready = NULL;
    if (err != ESP_OK) {
        gpio_reset_pin(s_config.gpio_num);
        s_task = NULL;
    } else {
        ESP_LOGI(TAG, "SETUP ready on GPIO%d (active LOW, internal pull-up)", s_config.gpio_num);
    }
    return err;
}
