#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialise the WS2812 on APP_LED_GPIO and start the status task.
 * Call once from app_main after camera and SD are initialised.
 */
esp_err_t status_led_init(void);

/* --- State setters called from other components / tasks --- */

/** Notify a JPEG was just saved to SD (produces a brief white pulse). */
void status_led_notify_shutter(void);

/** Reflect SoftAP state (called by main when toggling AP on/off). */
void status_led_set_ap_running(bool running);

/** Reflect whether a phone/client is connected to the AP. */
void status_led_set_client_connected(bool connected);

#ifdef __cplusplus
}
#endif
