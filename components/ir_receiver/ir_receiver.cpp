#include "ir_receiver.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <inttypes.h>
#include <new>

#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "IRac.h"
#include "IRrecv.h"
#include "IRutils.h"

namespace {

constexpr uint32_t TASK_STACK_BYTES = 8192;
constexpr size_t HEAP_RESERVE_BYTES = 48 * 1024;
#if CONFIG_FREERTOS_NUMBER_OF_CORES > 1
// GPIO interrupts are allocated on the core that installs the ISR service.
// Keep capture away from the Wi-Fi/Bluetooth work on CPU0 in Wired ESP32.
constexpr BaseType_t CAPTURE_CORE = 1;
#else
constexpr BaseType_t CAPTURE_CORE = 0;
#endif
static_assert(kStateSizeMax <= IR_RECEIVER_MAX_STATE_BYTES, "Increase public state capacity");

const char *TAG = "ir_receiver";
ir_receiver_config_t config;
IRrecv *receiver;
TaskHandle_t task;
SemaphoreHandle_t snapshot_lock;
ir_receiver_frame_t latest;
bool have_frame;

void log_raw(const ir_receiver_frame_t &frame, const ir_receiver_raw_view_t &raw)
{
    // Stream bounded lines instead of building a multi-kilobyte std::string.
    char line[224];
    size_t used = 0;
    for (size_t i = 0; i < raw.count; ++i) {
        const uint32_t duration_us = static_cast<uint32_t>(raw.ticks[i]) * raw.tick_us;
        const int written = snprintf(line + used, sizeof(line) - used, "%lu%s",
                                     static_cast<unsigned long>(duration_us),
                                     i + 1 == raw.count ? "" : ",");
        if (written < 0 || static_cast<size_t>(written) >= sizeof(line) - used) {
            ESP_LOGW(TAG, "RAW formatting failed for frame %lu", static_cast<unsigned long>(frame.sequence));
            return;
        }
        used += written;
        if ((i + 1) % 16 == 0 || i + 1 == raw.count) {
            ESP_LOGI(TAG, "RAW_US #%lu [%u]: %s", static_cast<unsigned long>(frame.sequence),
                     static_cast<unsigned>(i - (i % 16)), line);
            used = 0;
        }
    }
}

void handle_frame(const decode_results &result, uint32_t sequence)
{
    ir_receiver_frame_t frame = {};
    frame.sequence = sequence;
    frame.timestamp_us = esp_timer_get_time();
    frame.bits = result.bits;
    frame.raw_count = result.rawlen ? result.rawlen - 1 : 0;
    frame.overflow = result.overflow;
    frame.repeat = result.repeat;
    frame.known_protocol = result.decode_type != decode_type_t::UNKNOWN && !result.overflow;
    snprintf(frame.protocol, sizeof(frame.protocol), "%s", typeToString(result.decode_type).c_str());

    if (frame.known_protocol && hasACState(result.decode_type)) {
        const size_t state_bytes = (result.bits + 7U) / 8U;
        if (!state_bytes || state_bytes > sizeof(result.state) || state_bytes > sizeof(frame.state)) {
            frame.known_protocol = false;
            frame.overflow = true;
        } else {
            frame.has_ac_state = true;
            frame.state_length = state_bytes;
            memcpy(frame.state, result.state, frame.state_length);
        }
    } else if (frame.known_protocol) {
        // These fields share a union with state in decode_results.
        frame.value = result.value;
        frame.address = result.address;
        frame.command = result.command;
    }
    if (frame.known_protocol) {
        const auto description = IRAcUtils::resultAcToString(&result);
        frame.description_truncated = description.length() >= sizeof(frame.ac_description);
        snprintf(frame.ac_description, sizeof(frame.ac_description), "%s", description.c_str());
    }

    xSemaphoreTake(snapshot_lock, portMAX_DELAY);
    latest = frame;
    have_frame = true;
    xSemaphoreGive(snapshot_lock);

    const ir_receiver_raw_view_t raw = {
        .ticks = frame.raw_count ? result.rawbuf + 1 : nullptr,
        .count = frame.raw_count,
        .tick_us = kRawTick,
    };
    if (config.on_frame) {
        config.on_frame(&frame, &raw, config.user_ctx);
    }

    ESP_LOGI(TAG, "CAPTURA #%lu: %s, %u bits, %u tiempos, repeat=%u, overflow=%u",
             static_cast<unsigned long>(frame.sequence), frame.protocol, frame.bits,
             frame.raw_count, frame.repeat, frame.overflow);
    if (!frame.overflow) {
        ESP_LOGI(TAG, "%s", resultToHumanReadableBasic(&result).c_str());
    }
    if (frame.overflow) {
        ESP_LOGW(TAG, "Captura truncada; no usar como estado del aire. Aumenta el buffer si hay heap disponible.");
    } else if (frame.ac_description[0]) {
        ESP_LOGI(TAG, "Aire acondicionado: %s%s", frame.ac_description,
                 frame.description_truncated ? " [texto truncado; consultar bytes]" : "");
    } else if (!frame.known_protocol) {
        ESP_LOGI(TAG, "Protocolo desconocido; conserva RAW para analizarlo.");
    }
    if (frame.has_ac_state && frame.state_length) {
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, frame.state, frame.state_length, ESP_LOG_INFO);
    }
    if (config.dump_raw) {
        log_raw(frame, raw);
    }
}

void decoder_task(void *)
{
    // Stabilize an unplugged GPIO27. ESP32 input-only pins have no internal
    // pull-up, so leave it disabled there (an external bias may be needed).
    receiver->enableIRIn(GPIO_IS_VALID_OUTPUT_GPIO(config.gpio_num));
    ESP_LOGI(TAG, "OUT -> GPIO%d, capture core=%d. Esperando el control remoto...",
             config.gpio_num, xPortGetCoreID());
    decode_results result = {};
    uint32_t sequence = 0;
    for (;;) {
        if (receiver->decode(&result)) {
            handle_frame(result, ++sequence);
        }
        // IRremoteIDF resumes capture automatically using its save buffer.
        vTaskDelay(std::max<TickType_t>(1, pdMS_TO_TICKS(10)));
    }
}

} // namespace

extern "C" esp_err_t ir_receiver_init(const ir_receiver_config_t *requested)
{
    if (!requested || !GPIO_IS_VALID_GPIO(requested->gpio_num) ||
        requested->capture_buffer_entries < 1024 || requested->capture_buffer_entries > 16384 ||
        requested->frame_timeout_ms < 15 || requested->frame_timeout_ms > kMaxTimeoutMs) {
        return ESP_ERR_INVALID_ARG;
    }
    if (receiver) {
        return ESP_ERR_INVALID_STATE;
    }
    const size_t buffers = 2 * requested->capture_buffer_entries * sizeof(uint16_t);
    // Upstream constructors restart on buffer allocation failure. Check a
    // conservative budget and initialize before radio tasks consume heap.
    // This guard cannot guarantee allocation success under concurrent pressure.
    if (heap_caps_get_free_size(MALLOC_CAP_8BIT) < buffers + TASK_STACK_BYTES + HEAP_RESERVE_BYTES ||
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < buffers + 1024) {
        ESP_LOGE(TAG, "Insufficient heap for IR buffers and radio reserve");
        return ESP_ERR_NO_MEM;
    }
    snapshot_lock = xSemaphoreCreateMutex();
    if (!snapshot_lock) {
        return ESP_ERR_NO_MEM;
    }
    receiver = new (std::nothrow) IRrecv(requested->gpio_num, requested->capture_buffer_entries,
                                      requested->frame_timeout_ms, true);
    if (!receiver) {
        vSemaphoreDelete(snapshot_lock);
        snapshot_lock = nullptr;
        return ESP_ERR_NO_MEM;
    }
    config = *requested;
    ESP_LOGI(TAG, "IR decoder allocated: GPIO%d, buffer=%u, silence=%u ms",
             config.gpio_num, config.capture_buffer_entries, config.frame_timeout_ms);
    return ESP_OK;
}

extern "C" esp_err_t ir_receiver_start(void)
{
    if (!receiver) {
        return ESP_ERR_INVALID_STATE;
    }
    if (task) {
        return ESP_OK;
    }
    if (xTaskCreatePinnedToCore(decoder_task, "ir_receiver", TASK_STACK_BYTES, nullptr,
                              4, &task, CAPTURE_CORE) != pdPASS) {
        task = nullptr;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

extern "C" esp_err_t ir_receiver_get_latest(ir_receiver_frame_t *out_frame)
{
    if (!out_frame) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!receiver) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(snapshot_lock, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    const bool available = have_frame;
    if (available) {
        *out_frame = latest;
    }
    xSemaphoreGive(snapshot_lock);
    return available ? ESP_OK : ESP_ERR_INVALID_STATE;
}
