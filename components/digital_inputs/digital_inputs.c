#include "digital_inputs.h"

#include <stdint.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#if CONFIG_FREERTOS_NUMBER_OF_CORES > 1
#define INPUT_TASK_CORE 1
#else
#define INPUT_TASK_CORE 0
#endif

static const char *TAG = "digital_inputs";
ESP_EVENT_DEFINE_BASE(DIGITAL_INPUTS_EVENT);
static digital_inputs_config_t s_config;
static TaskHandle_t s_task;
static SemaphoreHandle_t s_snapshot_lock;
static portMUX_TYPE s_edge_lock = portMUX_INITIALIZER_UNLOCKED;
static uint32_t s_edge_bits;
static digital_input_state_t s_states[DIGITAL_INPUTS_MAX];
static bool s_valid[DIGITAL_INPUTS_MAX];
static bool s_start_attempted;
static esp_err_t s_startup_status = ESP_ERR_INVALID_STATE;

static void input_edge(void *arg)
{
    const unsigned index = (unsigned)(uintptr_t)arg;
    // The GPIO service uses flags=0, like IRremoteIDF: no cache-off execution.
    // No logging, GPIO sampling, debounce or application callback in the ISR.
    portENTER_CRITICAL_ISR(&s_edge_lock);
    s_edge_bits |= 1U << index;
    portEXIT_CRITICAL_ISR(&s_edge_lock);
    BaseType_t wake = pdFALSE;
    vTaskNotifyGiveFromISR(s_task, &wake);
    if (wake == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static uint32_t take_edge_bits(void)
{
    portENTER_CRITICAL(&s_edge_lock);
    const uint32_t bits = s_edge_bits;
    s_edge_bits = 0;
    portEXIT_CRITICAL(&s_edge_lock);
    return bits;
}

static bool edge_pending(unsigned index)
{
    portENTER_CRITICAL(&s_edge_lock);
    const bool pending = (s_edge_bits & (1U << index)) != 0;
    portEXIT_CRITICAL(&s_edge_lock);
    return pending;
}

static void publish_state(unsigned index, int level, int64_t now_us)
{
    digital_input_state_t state;
    xSemaphoreTake(s_snapshot_lock, portMAX_DELAY);
    if (s_valid[index] && s_states[index].level_high == (level != 0)) {
        xSemaphoreGive(s_snapshot_lock);
        return;
    }
    state = (digital_input_state_t) {
        .input_index = index,
        .gpio_num = s_config.inputs[index].gpio_num,
        .active = level == s_config.inputs[index].active_level,
        .level_high = level != 0,
        .initial = !s_valid[index],
        .change_count = s_valid[index] ? s_states[index].change_count + 1 : 0,
        .timestamp_us = now_us,
    };
    s_states[index] = state;
    s_valid[index] = true;
    xSemaphoreGive(s_snapshot_lock);

    const esp_err_t err = esp_event_post(DIGITAL_INPUTS_EVENT, DIGITAL_INPUTS_EVENT_CHANGED,
                                         &state, sizeof(state), 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Input %u event dropped: %s", index, esp_err_to_name(err));
    }
    if (s_config.on_change) {
        s_config.on_change(&state, s_config.user_ctx);
    } else {
        ESP_LOGI(TAG, "INPUT %u GPIO%d: %s, active=%u%s", index, state.gpio_num,
                 state.level_high ? "HIGH" : "LOW", state.active,
                 state.initial ? " [initial]" : "");
    }
}

static void input_task(void *arg)
{
    (void)arg;
    unsigned registered = 0;
    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        goto startup_failed;
    }
    // IR and this task both install on CPU1 in Wired. Whichever starts first
    // owns the shared service there; neither may uninstall it during cleanup.
    for (unsigned i = 0; i < s_config.input_count; ++i) {
        err = gpio_set_intr_type(s_config.inputs[i].gpio_num, GPIO_INTR_ANYEDGE);
        if (err != ESP_OK) {
            goto startup_failed;
        }
        err = gpio_isr_handler_add(s_config.inputs[i].gpio_num, input_edge, (void *)(uintptr_t)i);
        if (err != ESP_OK) {
            gpio_set_intr_type(s_config.inputs[i].gpio_num, GPIO_INTR_DISABLE);
            goto startup_failed;
        }
        ++registered;
    }
    xSemaphoreTake(s_snapshot_lock, portMAX_DELAY);
    s_startup_status = ESP_OK;
    xSemaphoreGive(s_snapshot_lock);
    ESP_LOGI(TAG, "%u inputs, GPIO ISR setup core=%d", s_config.input_count, xPortGetCoreID());

    int candidate[DIGITAL_INPUTS_MAX];
    int64_t since_us[DIGITAL_INPUTS_MAX];
    bool pending[DIGITAL_INPUTS_MAX];
    for (unsigned i = 0; i < s_config.input_count; ++i) {
        candidate[i] = gpio_get_level(s_config.inputs[i].gpio_num);
        since_us[i] = esp_timer_get_time();
        pending[i] = true; // Report initial state after settling, even with no edge.
    }
    for (;;) {
        TickType_t wait = portMAX_DELAY;
        int64_t now_us = esp_timer_get_time();
        for (unsigned i = 0; i < s_config.input_count; ++i) {
            if (!pending[i]) {
                continue;
            }
            const int64_t remaining = (int64_t)s_config.inputs[i].debounce_ms * 1000 - (now_us - since_us[i]);
            const TickType_t ticks = remaining <= 0 ? 0 :
                (TickType_t)((remaining * configTICK_RATE_HZ + 999999) / 1000000);
            if (ticks < wait) {
                wait = ticks;
            }
        }
        // Sleep indefinitely when stable, otherwise only until the nearest
        // input deadline. A bounce on one input does not reset the other.
        (void)ulTaskNotifyTake(pdTRUE, wait);
        const uint32_t edges = take_edge_bits();
        now_us = esp_timer_get_time();
        for (unsigned i = 0; i < s_config.input_count; ++i) {
            now_us = esp_timer_get_time();
            const int level = gpio_get_level(s_config.inputs[i].gpio_num);
            if ((edges & (1U << i)) || level != candidate[i]) {
                candidate[i] = level;
                since_us[i] = now_us;
                pending[i] = true;
            }
            if (pending[i] && now_us - since_us[i] >= (int64_t)s_config.inputs[i].debounce_ms * 1000 &&
                !edge_pending(i)) {
                publish_state(i, level, now_us);
                pending[i] = false;
            }
        }
    }

startup_failed:
    while (registered) {
        --registered;
        gpio_isr_handler_remove(s_config.inputs[registered].gpio_num);
        gpio_set_intr_type(s_config.inputs[registered].gpio_num, GPIO_INTR_DISABLE);
    }
    xSemaphoreTake(s_snapshot_lock, portMAX_DELAY);
    s_startup_status = err;
    xSemaphoreGive(s_snapshot_lock);
    ESP_LOGE(TAG, "GPIO startup failed: %s", esp_err_to_name(err));
    s_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t digital_inputs_init(const digital_inputs_config_t *config)
{
    if (!config || !config->input_count || config->input_count > DIGITAL_INPUTS_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_snapshot_lock) {
        return ESP_ERR_INVALID_STATE;
    }
    for (unsigned i = 0; i < config->input_count; ++i) {
        if (!GPIO_IS_VALID_GPIO(config->inputs[i].gpio_num) ||
            (config->inputs[i].active_level != 0 && config->inputs[i].active_level != 1) ||
            config->inputs[i].debounce_ms < 10 || config->inputs[i].debounce_ms > 1000) {
            return ESP_ERR_INVALID_ARG;
        }
        for (unsigned j = 0; j < i; ++j) {
            if (config->inputs[i].gpio_num == config->inputs[j].gpio_num) {
                return ESP_ERR_INVALID_ARG;
            }
        }
    }
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    for (unsigned i = 0; i < config->input_count; ++i) {
        const int pin = config->inputs[i].gpio_num;
        const gpio_config_t pin_config = {
            .pin_bit_mask = 1ULL << pin,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_IS_VALID_OUTPUT_GPIO(pin) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        const esp_err_t err = gpio_config(&pin_config);
        if (err != ESP_OK) {
            return err;
        }
    }
    s_snapshot_lock = xSemaphoreCreateMutex();
    if (!s_snapshot_lock) {
        return ESP_ERR_NO_MEM;
    }
    s_config = *config;
    return ESP_OK;
}

esp_err_t digital_inputs_start(void)
{
    if (!s_snapshot_lock || s_start_attempted) {
        return ESP_ERR_INVALID_STATE;
    }
    s_start_attempted = true;
    if (xTaskCreatePinnedToCore(input_task, "inputs", 3072, NULL, 4,
                              &s_task, INPUT_TASK_CORE) != pdPASS) {
        s_start_attempted = false;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t digital_inputs_get_state(unsigned index, digital_input_state_t *out_state)
{
    if (!out_state || index >= DIGITAL_INPUTS_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_snapshot_lock) {
        return ESP_ERR_INVALID_STATE;
    }
    if (index >= s_config.input_count) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(s_snapshot_lock, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t err = s_startup_status;
    if (err == ESP_OK) {
        err = s_valid[index] ? ESP_OK : ESP_ERR_INVALID_STATE;
        if (err == ESP_OK) {
            *out_state = s_states[index];
        }
    }
    xSemaphoreGive(s_snapshot_lock);
    return err;
}
