#pragma once

#include "esp_err.h"
#include "ir_receiver.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Application wiring/policy; reusable drivers remain independent. */
esp_err_t app_feedback_init(void);
void app_feedback_on_ir_frame(const ir_receiver_frame_t *frame,
                              const ir_receiver_raw_view_t *raw, void *user_ctx);

#ifdef __cplusplus
}
#endif
