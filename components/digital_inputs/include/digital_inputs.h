#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DIGITAL_INPUTS_MAX 5
ESP_EVENT_DECLARE_BASE(DIGITAL_INPUTS_EVENT);
enum { DIGITAL_INPUTS_EVENT_CHANGED };

typedef struct {
    unsigned input_index; /* Index in the configured array. */
    int gpio_num;
    bool active;         /* level matches the configured active_level. */
    bool level_high;
    bool initial;        /* Initial observation, not a button press command. */
    uint32_t change_count;
    int64_t timestamp_us; /* Stable confirmation time since boot. */
} digital_input_state_t;

/** Runs in the input task, after initial settling and stable changes.
 * Copy needed data and keep callbacks short. state is borrowed for this call.
 */
typedef void (*digital_inputs_callback_t)(const digital_input_state_t *state, void *user_ctx);

typedef struct {
    int gpio_num;
    int active_level;      /* 1 = NC circuit open; 0 = NO button pressed. */
    uint32_t debounce_ms;  /* 10 .. 1000, independently applied per input. */
} digital_input_config_t;

typedef struct {
    unsigned input_count; /* 1 .. DIGITAL_INPUTS_MAX */
    digital_input_config_t inputs[DIGITAL_INPUTS_MAX];
    digital_inputs_callback_t on_change; /* Optional; initial samples included. */
    void *user_ctx;
} digital_inputs_config_t;

/** Call once from startup. Copies config, initializes/reuses the default event
 * loop and configures input mode. Enables internal pull-ups on output-capable
 * pins; use external pull-ups on ESP32 input-only pins. The application checks
 * ownership conflicts with other components before init. No NVS or actuators.
 */
esp_err_t digital_inputs_init(const digital_inputs_config_t *config);

/** Call once after init. Creates an event-driven task on CPU1 for dual-core
 * ESP32, CPU0 for single-core targets. GPIO handlers are installed from that
 * task; the shared ISR service is reused and never uninstalled (IR uses it).
 * ESP_OK confirms task creation, not GPIO setup. Setup failures are logged
 * and returned by get_state. No stop/restart API is provided for this boot.
 */
esp_err_t digital_inputs_start(void);

/** Immediate snapshot, written only on ESP_OK. INVALID_STATE before the first
 * stable sample, TIMEOUT if the mutex is busy, or the GPIO startup error.
 */
esp_err_t digital_inputs_get_state(unsigned input_index, digital_input_state_t *out_state);

/* DIGITAL_INPUTS_EVENT_CHANGED posts a copied digital_input_state_t to the
 * default loop. Register consumers after init and before start. Initial states
 * have initial=true: do not interpret a button held at boot as a fresh command.
 * Event posting does not wait: a full queue can drop an event (logged); use
 * get_state to reconcile. There is no automatic repeat for held buttons.
 */
#ifdef __cplusplus
}
#endif
