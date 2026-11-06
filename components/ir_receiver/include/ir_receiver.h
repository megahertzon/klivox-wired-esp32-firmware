#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IR_RECEIVER_PROTOCOL_NAME_SIZE 32
#define IR_RECEIVER_DESCRIPTION_SIZE 384
#define IR_RECEIVER_MAX_STATE_BYTES 64

typedef struct {
    uint32_t sequence;
    int64_t timestamp_us; /* Monotonic time at decode, not first optical edge. */
    char protocol[IR_RECEIVER_PROTOCOL_NAME_SIZE];
    char ac_description[IR_RECEIVER_DESCRIPTION_SIZE];
    uint16_t bits;
    uint16_t raw_count; /* Excludes the synthetic leading gap. */
    bool known_protocol;
    bool overflow;      /* Truncated captures must not drive automation. */
    bool repeat;
    bool has_ac_state;
    bool description_truncated;
    uint8_t state[IR_RECEIVER_MAX_STATE_BYTES];
    uint16_t state_length;
    /* Only interpreted for known protocols without a multi-byte AC state.
     * UNKNOWN hashes are deliberately not exposed as remote commands. */
    uint64_t value;
    uint32_t address;
    uint32_t command;
} ir_receiver_frame_t;

typedef struct {
    /* Borrowed snapshot, stable only for the duration of on_frame. */
    const volatile uint16_t *ticks;
    size_t count;
    uint16_t tick_us; /* duration_us = (uint32_t)ticks[i] * tick_us */
    /* Even entries are active-low marks; odd entries are spaces. */
} ir_receiver_raw_view_t;

/** Runs in the decoder task. Copy needed data or enqueue work; do not retain
 * pointers or call network/Zigbee operations that block this task. A capture
 * is observational: it does not confirm the AC actually received the command.
 */
typedef void (*ir_receiver_callback_t)(const ir_receiver_frame_t *frame,
                                     const ir_receiver_raw_view_t *raw, void *user_ctx);

typedef struct {
    int gpio_num;
    uint16_t capture_buffer_entries; /* 1024 .. 16384 */
    uint8_t frame_timeout_ms;        /* 15 .. 130 */
    bool dump_raw;
    ir_receiver_callback_t on_frame; /* Optional. */
    void *user_ctx;
} ir_receiver_config_t;

/** Allocate the singleton decoder. Call serially from startup BEFORE Wi-Fi/BLE
 * starts allocating heap. The copied config/context remains fixed for this boot.
 * No GPIO is enabled until start. IRremoteIDF owns one receiver process-wide.
 * The library uses C++ internally; this public API can be consumed from C.
 */
esp_err_t ir_receiver_init(const ir_receiver_config_t *config);

/** Create the decoder task once. Repeated successful starts are harmless.
 * Low-level GPIO/timer startup follows IRremoteIDF's void enableIRIn API: errors
 * there are logged by the library. ESP_OK means the task was created, not that
 * hardware reception has been verified. No transmitter or automation is started.
 */
esp_err_t ir_receiver_start(void);

/** Non-blocking, thread-safe copy of the last capture, without RAW pointers.
 * ESP_ERR_INVALID_STATE: no capture yet; ESP_ERR_TIMEOUT: snapshot lock busy.
 * Output is written only on ESP_OK. Inspect known_protocol/overflow before use;
 * even ESP_OK does not mean a recognized AC protocol or confirmed appliance state.
 */
esp_err_t ir_receiver_get_latest(ir_receiver_frame_t *out_frame);

#ifdef __cplusplus
}
#endif
