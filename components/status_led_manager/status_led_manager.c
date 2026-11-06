#include "status_led_manager.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#define LED_MODE LEDC_LOW_SPEED_MODE
#define LED_TIMER LEDC_TIMER_0
#define LED_MAX 255U
#define LED_FAST_MS 250U
#define LED_SLOW_MS 1000U

static const char *TAG = "KLIVOX_LED";
static const int s_pins[] = {CONFIG_KLIVOX_LED_RED_GPIO,
                             CONFIG_KLIVOX_LED_GREEN_GPIO,
                             CONFIG_KLIVOX_LED_BLUE_GPIO,
                             CONFIG_KLIVOX_LED_ANODE_GPIO};
static const ledc_channel_t s_channels[] = {LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2};

typedef struct {
    const char *name;
    uint8_t rgb[3];
    uint16_t half_period_ms;
} pattern_t;

static const pattern_t s_patterns[STATUS_LED_STATE_COUNT] = {
    [STATUS_LED_BOOTING] = {"BOOTING", {255, 255, 0}, LED_SLOW_MS},
    [STATUS_LED_BLE_PROVISIONING] = {"BLE_PROVISIONING", {0, 0, 255}, LED_FAST_MS},
    [STATUS_LED_WIFI_CONNECTING] = {"WIFI_CONNECTING", {255, 255, 0}, LED_FAST_MS},
    [STATUS_LED_MQTT_CONNECTING] = {"MQTT_CONNECTING", {0, 0, 255}, LED_SLOW_MS},
    [STATUS_LED_ONLINE] = {"ONLINE", {0, 255, 0}, 0},
    [STATUS_LED_OFFLINE] = {"OFFLINE", {255, 0, 0}, LED_SLOW_MS},
    [STATUS_LED_ERROR] = {"ERROR", {255, 0, 0}, LED_FAST_MS},
    [STATUS_LED_WIFI_CONNECTED] = {"WIFI_CONNECTED", {0, 255, 255}, 0},
    [STATUS_LED_OFF] = {"OFF", {0, 0, 0}, 0},
};

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_attempted;
static esp_err_t s_result = ESP_ERR_INVALID_STATE;
static TaskHandle_t s_task;
static status_led_state_t s_base = STATUS_LED_BOOTING;
static bool s_error;
static bool s_provisioning;
static uint32_t s_conditions;
static uint8_t s_brightness = CONFIG_KLIVOX_LED_BRIGHTNESS;
#ifdef CONFIG_KLIVOX_LED_HIDE_ONLINE
static bool s_online_visible;
#else
static bool s_online_visible = true;
#endif

const char *status_led_manager_state_name(status_led_state_t state)
{
    return state >= STATUS_LED_BOOTING && state < STATUS_LED_STATE_COUNT
           ? s_patterns[state].name : "INVALID";
}

static bool valid_base(status_led_state_t state)
{
    return state >= STATUS_LED_BOOTING && state < STATUS_LED_STATE_COUNT &&
           state != STATUS_LED_ERROR && state != STATUS_LED_BLE_PROVISIONING;
}

/* Equal-priority conditions use this deterministic order. The connectivity
 * owner replaces its mutually exclusive base; other owners may assert/release
 * conditions explicitly without suppressing updates beneath them. */
static status_led_state_t effective_state(void)
{
    static const status_led_state_t order[] = {
        STATUS_LED_ERROR, STATUS_LED_BLE_PROVISIONING, STATUS_LED_OFFLINE,
        STATUS_LED_WIFI_CONNECTING, STATUS_LED_MQTT_CONNECTING,
        STATUS_LED_WIFI_CONNECTED, STATUS_LED_ONLINE, STATUS_LED_BOOTING, STATUS_LED_OFF,
    };
    uint32_t active = s_conditions | (1U << s_base);
    if (s_error) active |= 1U << STATUS_LED_ERROR;
    if (s_provisioning) active |= 1U << STATUS_LED_BLE_PROVISIONING;
    for (unsigned i = 0; i < sizeof(order) / sizeof(order[0]); ++i) {
        if (active & (1U << order[i])) return order[i];
    }
    return STATUS_LED_OFF;
}

static esp_err_t render(status_led_state_t state, uint8_t brightness, bool on)
{
    for (unsigned i = 0; i < 3; ++i) {
        const uint32_t logical = on ? (s_patterns[state].rgb[i] * brightness) / LED_MAX : 0;
        /* Common anode: HIGH is off. All LEDC writes belong to this task. */
        esp_err_t err = ledc_set_duty(LED_MODE, s_channels[i], LED_MAX - logical);
        if (err == ESP_OK) {
            err = ledc_update_duty(LED_MODE, s_channels[i]);
        }
        if (err != ESP_OK) {
            for (unsigned j = 0; j < 3; ++j) {
                (void)ledc_stop(LED_MODE, s_channels[j], 1);
            }
            return err;
        }
    }
    return ESP_OK;
}

static void led_task(void *arg)
{
    (void)arg;
    /* init publishes readiness only after the task handle is valid. */
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    status_led_state_t previous = STATUS_LED_STATE_COUNT;
    uint8_t previous_brightness = 0;
    bool previous_visible = false;
    bool on = true;
    bool notified = true;
    TickType_t deadline = 0;
    for (;;) {
        portENTER_CRITICAL(&s_lock);
        status_led_state_t state = effective_state();
        uint8_t brightness = s_brightness;
        bool visible = state != STATUS_LED_ONLINE || s_online_visible;
        portEXIT_CRITICAL(&s_lock);
        const TickType_t half_period = pdMS_TO_TICKS(s_patterns[state].half_period_ms);
        const TickType_t now = xTaskGetTickCount();
        bool changed = state != previous || brightness != previous_brightness || visible != previous_visible;
        if (changed) {
            if (state != previous) {
                ESP_LOGI(TAG, "State changed: %s", s_patterns[state].name);
            }
            on = true;
            deadline = now + half_period;
        } else if (half_period && (!notified || (int32_t)(deadline - now) <= 0)) {
            on = !on;
            deadline = now + half_period;
        }
        if (changed || !notified || half_period) {
            esp_err_t err = render(state, brightness, on && visible);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "PWM update failed, outputs disabled: %s", esp_err_to_name(err));
                /* No spinning/log flood on hardware failure; next update retries. */
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                previous = STATUS_LED_STATE_COUNT;
                notified = true;
                continue;
            }
        }
        previous = state;
        previous_brightness = brightness;
        previous_visible = visible;
        TickType_t wait = portMAX_DELAY;
        if (half_period) {
            int32_t remaining = (int32_t)(deadline - xTaskGetTickCount());
            wait = remaining > 0 ? (TickType_t)remaining : 1;
        }
        notified = ulTaskNotifyTake(pdTRUE, wait) != 0;
    }
}

esp_err_t status_led_manager_init(void)
{
    portENTER_CRITICAL(&s_lock);
    bool attempted = s_attempted;
    s_attempted = true;
    esp_err_t result = s_result;
    portEXIT_CRITICAL(&s_lock);
    if (attempted) {
        return result;
    }
    unsigned configured = 0;
    bool timer_configured = false;
    for (unsigned i = 0; i < sizeof(s_pins) / sizeof(s_pins[0]); ++i) {
        if (!GPIO_IS_VALID_OUTPUT_GPIO(s_pins[i])) {
            result = ESP_ERR_INVALID_ARG;
            goto done;
        }
        for (unsigned j = 0; j < i; ++j) {
            if (s_pins[i] == s_pins[j]) {
                result = ESP_ERR_INVALID_ARG;
                goto done;
            }
        }
    }
    for (unsigned i = 0; i < 3; ++i) {
        result = gpio_set_level(s_pins[i], 1);
        if (result != ESP_OK) goto done;
        result = gpio_set_direction(s_pins[i], GPIO_MODE_OUTPUT);
        if (result != ESP_OK) goto done;
    }
    const ledc_timer_config_t timer = {
        .speed_mode = LED_MODE, .timer_num = LED_TIMER,
        .duty_resolution = LEDC_TIMER_8_BIT, .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    result = ledc_timer_config(&timer);
    if (result != ESP_OK) goto done;
    timer_configured = true;
    for (unsigned i = 0; i < 3; ++i) {
        const ledc_channel_config_t channel = {
            .gpio_num = s_pins[i], .speed_mode = LED_MODE,
            .channel = s_channels[i], .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LED_TIMER, .duty = LED_MAX, .hpoint = 0,
        };
        result = ledc_channel_config(&channel);
        if (result != ESP_OK) goto done;
        ++configured;
    }
    /* Supply the common anode only after all cathodes are off. This pin is
     * never PWM-controlled or changed by state/brightness/night-mode updates. */
    result = gpio_set_level(CONFIG_KLIVOX_LED_ANODE_GPIO, 1);
    if (result != ESP_OK) goto done;
    result = gpio_set_direction(CONFIG_KLIVOX_LED_ANODE_GPIO, GPIO_MODE_OUTPUT);
    if (result != ESP_OK) goto done;
    ESP_LOGI(TAG, "Common anode GPIO%d held HIGH", CONFIG_KLIVOX_LED_ANODE_GPIO);
    if (xTaskCreate(led_task, "status_led", 3072, NULL, 1, &s_task) != pdPASS) {
        result = ESP_ERR_NO_MEM;
        goto done;
    }
    result = ESP_OK;
done:
    if (result != ESP_OK) {
        for (unsigned i = 0; i < configured; ++i) {
            (void)ledc_stop(LED_MODE, s_channels[i], 1);
        }
        if (timer_configured) (void)ledc_timer_pause(LED_MODE, LED_TIMER);
    }
    portENTER_CRITICAL(&s_lock);
    s_result = result;
    portEXIT_CRITICAL(&s_lock);
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "Initialized RGB LED (common anode)");
        xTaskNotifyGive(s_task);
    }
    return result;
}

esp_err_t status_led_manager_set_state(status_led_state_t state)
{
    if (state == STATUS_LED_ERROR || state == STATUS_LED_BLE_PROVISIONING) {
        return status_led_manager_set_condition(state, true);
    }
    if (!valid_base(state)) return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&s_lock);
    esp_err_t err = s_result;
    if (err == ESP_OK) s_base = state;
    portEXIT_CRITICAL(&s_lock);
    if (err == ESP_OK) xTaskNotifyGive(s_task);
    return err;
}

esp_err_t status_led_manager_set_condition(status_led_state_t state, bool active)
{
    if (state < STATUS_LED_BOOTING || state >= STATUS_LED_STATE_COUNT) return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&s_lock);
    esp_err_t err = s_result;
    if (err == ESP_OK) {
        if (state == STATUS_LED_ERROR) s_error = active;
        else if (state == STATUS_LED_BLE_PROVISIONING) s_provisioning = active;
        else if (active) s_conditions |= 1U << state;
        else s_conditions &= ~(1U << state);
    }
    portEXIT_CRITICAL(&s_lock);
    if (err == ESP_OK) xTaskNotifyGive(s_task);
    return err;
}

esp_err_t status_led_manager_set_connectivity(status_led_state_t base, bool provisioning)
{
    if (!valid_base(base)) return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&s_lock);
    esp_err_t err = s_result;
    if (err == ESP_OK) {
        s_base = base;
        s_provisioning = provisioning;
    }
    portEXIT_CRITICAL(&s_lock);
    if (err == ESP_OK) xTaskNotifyGive(s_task);
    return err;
}

status_led_state_t status_led_manager_get_state(void)
{
    portENTER_CRITICAL(&s_lock);
    status_led_state_t state = effective_state();
    portEXIT_CRITICAL(&s_lock);
    return state;
}

esp_err_t status_led_manager_set_brightness(uint8_t brightness)
{
    portENTER_CRITICAL(&s_lock);
    esp_err_t err = s_result;
    if (err == ESP_OK) s_brightness = brightness;
    portEXIT_CRITICAL(&s_lock);
    if (err == ESP_OK) xTaskNotifyGive(s_task);
    return err;
}

esp_err_t status_led_manager_set_online_visible(bool visible)
{
    portENTER_CRITICAL(&s_lock);
    esp_err_t err = s_result;
    if (err == ESP_OK) s_online_visible = visible;
    portEXIT_CRITICAL(&s_lock);
    if (err == ESP_OK) xTaskNotifyGive(s_task);
    return err;
}
