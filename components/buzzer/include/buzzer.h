#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int gpio_num; /* Active-high transistor control; buzzer is externally powered. */
} buzzer_config_t;

/** Initialize from one startup task; preload OFF before enabling the output.
 * Repeated init with the same GPIO is harmless. GPIO conflicts are checked by
 * the application. Timer/semaphore live for this boot. No startup beep.
 */
esp_err_t buzzer_init(const buzzer_config_t *config);

/** Request a 50..1000 ms pulse, from task context (not ISR).
 * Returns immediately. ESP_ERR_TIMEOUT means already sounding: no queuing,
 * restart or extension. An esp_timer callback switches the GPIO off; actual
 * pulse length may increase if timer dispatch is delayed under system load.
 * Unlike the old on_off_light buzzer_beep API, this function NEVER waits for
 * the pulse to finish, so it can be used from the IR decoder's task callback.
 */
esp_err_t buzzer_beep(uint32_t duration_ms);

#ifdef __cplusplus
}
#endif
